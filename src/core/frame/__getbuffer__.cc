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
#include <exception>
#include <stdlib.h>  // atoi
#include "column/date_from_weeks.h"
#include "column/date_from_months.h"
#include "column/date_from_years.h"
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

std::exception_ptr getbuffer_exception;


// Forward declarations
static void copy_column_into_buffer(const Column& col, Buffer& buf);

#define REQ_ND(flags)       ((flags & PyBUF_ND) == PyBUF_ND)
#define REQ_FORMAT(flags)   ((flags & PyBUF_FORMAT) == PyBUF_FORMAT)
#define REQ_STRIDES(flags)  ((flags & PyBUF_STRIDES) == PyBUF_STRIDES)
#define REQ_WRITABLE(flags) ((flags & PyBUF_WRITABLE) == PyBUF_WRITABLE)


/**
  * Helper structure that stores information exported with the buffer.
  * This structure is then passed along with the `view`, using the
  * field `view->internal`.
  */
struct XInfo {
  // Exported Buffer object.
  Buffer mbuf;

  // An array of integers of length `ndim`, indicating the shape of the
  // memory as an n-dimensional array (prod(shape) * itemsize == len).
  // Must be provided iff PyBUF_ND is set.
  Py_ssize_t shape[2];

  // An array of integers of length `ndim` giving the number of bytes
  // to skip to get to a new element in each dimension.
  // Must be provided iff PyBUF_STRIDES is set.
  Py_ssize_t strides[2];

  XInfo() {
    shape[0] = shape[1] = 0;
    strides[0] = strides[1] = 0;
  }
};



static int getbuffer_0_cols(py::Frame* pydt, Py_buffer* view, int flags) {
  XInfo* xinfo = new XInfo();
  view->buf = nullptr;
  view->obj = py::oobj(pydt).release();
  view->len = 0;
  view->readonly = 0;
  view->itemsize = 1;
  view->format = REQ_FORMAT(flags) ? const_cast<char*>("B") : nullptr;
  view->ndim = 2;
  view->shape = REQ_ND(flags) ? xinfo->shape : nullptr;
  view->strides = REQ_STRIDES(flags) ? xinfo->strides : nullptr;
  view->suboffsets = nullptr;
  view->internal = xinfo;
  return 0;
}


int py::Frame::m__getbuffer__(Py_buffer* view, int flags) noexcept {
  try {
    XInfo* xinfo = nullptr;
    size_t ncols = dt->ncols();
    size_t nrows = dt->nrows();
    if (ncols == 0) {
      return getbuffer_0_cols(this, view, flags);
    }

    // Multiple columns datatable => copy all data into a new buffer before
    // passing it to the requester. This is of course very unfortunate, but
    // Numpy (the primary consumer of the buffer protocol) is unable to handle
    // "INDIRECT" buffer.
    // Also note that numpy will request an INDIRECT buffer, even though it is
    // unable to handle it. See https://github.com/numpy/numpy/issues/9456
    // For now, we just ignore the INDIRECT flag completely and return the
    // contiguous buffer always.

    // First, find the common stype for all columns in the DataTable.
    dt::Type type;
    if (!type) {
      for (size_t i = 0; i < ncols; ++i) {
        type.promote(dt->get_column(i).type());
      }
    }
    xassert(type);
    if (type.is_invalid()) {
      throw TypeError() <<
          "Frame contains columns of incompatible types and cannot be "
          "converted into a numpy array";
    }
    if (type.is_string()) {
      type = dt::Type::obj64();
    }

    // Allocate the final buffer
    auto stype = type.stype();
    xassert(!stype_is_variable_width(stype));
    size_t elemsize = stype_elemsize(stype);
    size_t colsize = nrows * elemsize;
    const char* fmt = type.struct_format();
    Buffer memr;

    const Column& col0 = dt->get_column(0);
    if (ncols == 1 && col0.get_num_data_buffers() == 1 && !REQ_WRITABLE(flags)) {
      memr = col0.get_data_buffer(0);
    }
    else {
      memr = Buffer::mem(ncols * colsize);
      // Construct the data buffer
      for (size_t i = 0; i < ncols; ++i) {
        // xmb becomes a "view" on a portion of the buffer `mbuf`. An
        // ExternalMemBuf object is documented to be readonly; however in
        // practice it can still be written to, just not resized (this is
        // hacky, maybe fix in the future).
        Buffer xmb = Buffer::view(memr, colsize, i*colsize);
        // Now we create a `newcol` by casting `col` into `stype`, using
        // the buffer `xmb`. Since `xmb` already has the correct size, this
        // is possible. The effect of this call is that `newcol` will be
        // created having the converted data; but the side-effect of this is
        // that `mbuf` will have the same data, and in the right place.
        {
          Column newcol = dt->get_column(i);
          newcol.cast_inplace(stype);
          copy_column_into_buffer(newcol, xmb);
          // We can now delete the new column: this will delete `xmb` as well,
          // however a "view buffer" object does not attempt to free its
          // memory. The converted data that was written to `xmb` will
          // thus remain intact.
        }
      }
      if (type.is_object()) {
        memr.set_pyobjects(/*clear=*/ false);
      }
    }

    xinfo = new XInfo();
    xinfo->mbuf = std::move(memr);
    xinfo->shape[0] = static_cast<Py_ssize_t>(nrows);
    xinfo->shape[1] = static_cast<Py_ssize_t>(ncols);
    xinfo->strides[0] = static_cast<Py_ssize_t>(elemsize);
    xinfo->strides[1] = static_cast<Py_ssize_t>(colsize);

    // Fill in the `view` struct
    view->buf = const_cast<void*>(xinfo->mbuf.rptr());
    view->obj = py::oobj(this).release();
    view->len = static_cast<Py_ssize_t>(xinfo->mbuf.size());
    view->readonly = 0;
    view->itemsize = static_cast<Py_ssize_t>(elemsize);
    view->format = REQ_FORMAT(flags) ? const_cast<char*>(fmt) : nullptr;
    view->ndim = 2;
    view->shape = REQ_ND(flags)? xinfo->shape : nullptr;
    view->strides = REQ_STRIDES(flags)? xinfo->strides : nullptr;
    view->suboffsets = nullptr;
    view->internal = xinfo;
    return 0;

  } catch (const std::exception& e) {
    exception_to_python(e);
    getbuffer_exception = std::current_exception();
    return -1;
  }
}

void py::Frame::m__releasebuffer__(Py_buffer* view) noexcept {
  XInfo* xinfo = static_cast<XInfo*>(view->internal);
  delete xinfo;
}




//==============================================================================
// Buffers utility functions
//==============================================================================

template <typename T>
static void _copy_column_fw(const Column& col, Buffer& buf) {
  xassert(col.can_be_read_as<T>());
  xassert(buf.size() == col.nrows() * sizeof(T));
  auto nthreads = dt::NThreads(col.allow_parallel_access());
  auto out_data = static_cast<T*>(buf.wptr());

  dt::parallel_for_static(col.nrows(), nthreads,
    [&](size_t i) {
      T value;
      bool isvalid = col.get_element(i, &value);
      out_data[i] = isvalid? value : dt::GETNA<T>();
    });
}

static void _copy_column_obj(const Column& col, Buffer& buf) {
  xassert(col.can_be_read_as<py::oobj>());
  xassert(buf.size() == col.nrows() * sizeof(py::oobj));
  xassert(!buf.is_pyobjects());
  auto out_data = reinterpret_cast<PyObject**>(buf.wptr());

  for (size_t i = 0; i < col.nrows(); ++i) {
    py::oobj value;
    bool isvalid = col.get_element(i, &value);
    out_data[i] = (isvalid? std::move(value) : py::None()).release();
  }
  buf.set_pyobjects(false);
  xassert(buf.is_pyobjects());
}

static void copy_column_into_buffer(const Column& col, Buffer& buf) {
  switch (col.stype()) {
    case dt::SType::BOOL:
    case dt::SType::INT8:    _copy_column_fw<int8_t>(col, buf); break;
    case dt::SType::INT16:   _copy_column_fw<int16_t>(col, buf); break;
    case dt::SType::INT32:   _copy_column_fw<int32_t>(col, buf); break;
    case dt::SType::TIME64:
    case dt::SType::INT64:   _copy_column_fw<int64_t>(col, buf); break;
    case dt::SType::FLOAT32: _copy_column_fw<float>(col, buf); break;
    case dt::SType::FLOAT64: _copy_column_fw<double>(col, buf); break;
    case dt::SType::OBJ:     _copy_column_obj(col, buf); break;
    default: throw RuntimeError() << "Cannot write " << col.stype()
                << " values into a numpy array";
  }
}
