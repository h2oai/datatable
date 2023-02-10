//------------------------------------------------------------------------------
// Copyright 2018-2023 H2O.ai
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
    res = res.reduce_type(/*strict=*/false);
  }
  return res;
}


static Column convert_fwchararray_to_column(py::buffer&& view)
{
  // When calculating `k` (the number of characters in an element) and
  // `maxsize` (the maximum size of the string buffer), we take into account
  // that in numpy the size of each unicode character is 4 bytes.
  size_t k = view.itemsize() / 4;
  size_t nrows = view.nelements();
  size_t stride = view.stride() * k;
  size_t maxsize = nrows * k * 4;
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
