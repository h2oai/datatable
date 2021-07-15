//------------------------------------------------------------------------------
// Copyright 2018-2021 H2O.ai
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the "Software"),
// to deal in the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense,
// and/or sell copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.
//------------------------------------------------------------------------------
// Functionality related to "Buffers" interface
// See: https://www.python.org/dev/peps/pep-3118/
// See: https://docs.python.org/3/c-api/buffer.html
//------------------------------------------------------------------------------
#include <stdlib.h>  // atoi
#include "column/date_from_weeks.h"
#include "column/date_from_months.h"
#include "column/date_from_years.h"
#include "column/time_scaled.h"
#include "column/view.h"
#include "frame/py_frame.h"
#include "parallel/api.h"
#include "python/obj.h"
#include "python/string.h"
#include "python/pybuffer.h"
#include "utils/assert.h"
#include "utils/exceptions.h"
#include "column.h"
#include "datatablemodule.h"
#include "encodings.h"
#include "stype.h"

// Forward declarations
static void try_to_resolve_object_column(Column& col);
static Column convert_fwchararray_to_column(py::buffer&& view);




//------------------------------------------------------------------------------
// Construct a Column from a python object implementing Buffers protocol
//------------------------------------------------------------------------------

Column Column::from_pybuffer(const py::robj& pyobj) {
  // Check if this is a datetime64 column, in which case it must be converted
  if (pyobj.is_numpy_array()) {
    auto dtype = pyobj.get_attr("dtype");
    auto kind = dtype.get_attr("kind").to_string();
    if (kind == "M") {  // datetime64
      // datetime64 column does not support PyBuffers interface, so it has to
      // be cast into int64 first.
      Column tmp = Column::from_pybuffer(
        pyobj.invoke("view", py::ostring("int64"))
      );
      xassert(tmp.stype() == dt::SType::INT64);
      auto str = dtype.get_attr("str").to_string();
      if (str == "<M8[D]") {
        tmp.cast_inplace(dt::SType::DATE32);
        return tmp;
      }
      if (str == "<M8[ns]") {
        tmp.cast_inplace(dt::SType::TIME64);
        return tmp;
      }
      if (str == "<M8[W]") return Column(new dt::DateFromWeeks_ColumnImpl(std::move(tmp)));
      if (str == "<M8[M]") return Column(new dt::DateFromMonths_ColumnImpl(std::move(tmp)));
      if (str == "<M8[Y]") return Column(new dt::DateFromYears_ColumnImpl(std::move(tmp)));
      constexpr int64_t NANOS    = 1;
      constexpr int64_t MICROS   = 1000 * NANOS;
      constexpr int64_t MILLIS   = 1000000 * NANOS;
      constexpr int64_t SECONDS  = 1000000000 * NANOS;
      constexpr int64_t MINUTES  = 60 * SECONDS;
      constexpr int64_t HOURS    = 3600 * SECONDS;
      if (str == "<M8[h]") return Column(new dt::TimeScaled_ColumnImpl(std::move(tmp), HOURS));
      if (str == "<M8[m]") return Column(new dt::TimeScaled_ColumnImpl(std::move(tmp), MINUTES));
      if (str == "<M8[s]") return Column(new dt::TimeScaled_ColumnImpl(std::move(tmp), SECONDS));
      if (str == "<M8[ms]") return Column(new dt::TimeScaled_ColumnImpl(std::move(tmp), MILLIS));
      if (str == "<M8[us]") return Column(new dt::TimeScaled_ColumnImpl(std::move(tmp), MICROS));
    }
    if (kind == "M" || kind == "m") {
      return Column::from_pybuffer(pyobj.invoke("astype", py::ostring("str")));
    }
    auto fmt = dtype.get_attr("char").to_string();
    if (kind == "f" && fmt == "e") {  // float16
      return Column::from_pybuffer(pyobj.invoke("astype", py::ostring("float32")));
    }
  }

  py::buffer view(pyobj);

  Column res;
  if (view.stype() == dt::SType::STR32) {
    res = convert_fwchararray_to_column(std::move(view));
  }
  else {
    res = std::move(view).to_column();
  }

  if (res.type().is_object()) {

    try_to_resolve_object_column(res);
  }
  return res;
}


static Column convert_fwchararray_to_column(py::buffer&& view)
{
  // Number of characters in each element (each Unicode character is 4 bytes
  // in numpy).
  size_t k = view.itemsize() / 4;
  size_t nrows = view.nelements();
  size_t stride = view.stride() * k;
  size_t maxsize = nrows * k;
  auto input = static_cast<uint32_t*>(view.data());

  Buffer strbuf = Buffer::mem(maxsize);
  Buffer offbuf = Buffer::mem((nrows + 1) * 4);
  auto strptr = static_cast<char*>(strbuf.wptr());
  auto offptr = static_cast<uint32_t*>(offbuf.wptr());
  *offptr++ = 0;
  uint32_t offset = 0;
  for (size_t j = 0; j < nrows; ++j) {
    uint32_t* start = input + j*stride;  // note: stride can be negative!
    int64_t bytes_len = utf32_to_utf8(start, k, strptr);
    offset += static_cast<uint32_t>(bytes_len);
    strptr += bytes_len;
    *offptr++ = offset;
  }

  strbuf.resize(static_cast<size_t>(offset));
  return Column::new_string_column(nrows, std::move(offbuf), std::move(strbuf));
}



/**
 * If a column was created from Pandas series, it may end up having
 * dtype='object' (because Pandas doesn't support string columns). This function
 * will attempt to convert such column to the string type (or some other type
 * if more appropriate), and return either the original or the new modified
 * column. If a new column is returned, the original one is decrefed.
 */
static void try_to_resolve_object_column(Column& col)
{
  auto data = static_cast<PyObject* const*>(col.get_data_readonly());
  size_t nrows = col.nrows();

  bool all_strings = true;
  bool all_booleans = true;

  // Approximate total length of all strings. Do not take into account
  // possibility that the strings may expand in UTF-8 -- if needed, we'll
  // realloc the buffer later.
  size_t total_length = 10;
  for (size_t i = 0; i < nrows; ++i) {
    PyObject* v = data[i];
    if (v == Py_None) {}
    else if (all_strings && PyUnicode_Check(v)) {
      all_booleans = false;
      total_length += static_cast<size_t>(PyUnicode_GetLength(v));
    }
    else if (all_booleans && (v == Py_False || v == Py_True)) {
      all_strings = false;
    }
    else if (PyFloat_Check(v) && std::isnan(PyFloat_AS_DOUBLE(v))) {}
    else {
      all_strings = false;
      all_booleans = false;
      break;
    }
  }

  // All values in the list were booleans (or None)
  if (all_booleans) {
    Buffer mbuf = Buffer::mem(nrows);
    auto out = static_cast<int8_t*>(mbuf.xptr());
    for (size_t i = 0; i < nrows; ++i) {
      PyObject* v = data[i];
      out[i] = v == Py_True? 1 : v == Py_False? 0 : dt::GETNA<int8_t>();
    }
    col = Column::new_mbuf_column(nrows, dt::SType::BOOL, std::move(mbuf));
  }

  // All values were strings
  else if (all_strings) {
    size_t strbuf_size = total_length;
    Buffer offbuf = Buffer::mem((nrows + 1) * 4);
    Buffer strbuf = Buffer::mem(strbuf_size);
    uint32_t* offsets = static_cast<uint32_t*>(offbuf.xptr());
    char* strs = static_cast<char*>(strbuf.xptr());

    offsets[0] = 0;
    ++offsets;

    uint32_t offset = 0;
    for (size_t i = 0; i < nrows; ++i) {
      PyObject* v = data[i];
      if (PyUnicode_Check(v)) {
        PyObject *z = PyUnicode_AsEncodedString(v, "utf-8", "strict");
        size_t sz = static_cast<size_t>(PyBytes_Size(z));
        if (offset + sz > strbuf_size) {
          strbuf_size = static_cast<size_t>(
              1.5 * static_cast<double>(strbuf_size) + static_cast<double>(sz));
          strbuf.resize(strbuf_size);
          strs = static_cast<char*>(strbuf.xptr());
        }
        std::memcpy(strs + offset, PyBytes_AsString(z), sz);
        Py_DECREF(z);
        offset += static_cast<uint32_t>(sz);
        offsets[i] = offset;
      } else {
        offsets[i] = offset ^ dt::GETNA<uint32_t>();
      }
    }

    xassert(offset <= strbuf.size());
    strbuf.resize(offset);
    col = Column::new_string_column(nrows, std::move(offbuf), std::move(strbuf));
  }
}
