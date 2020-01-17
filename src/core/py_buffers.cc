//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
// Functionality related to "Buffers" interface
// See: https://www.python.org/dev/peps/pep-3118/
// See: https://docs.python.org/3/c-api/buffer.html
//------------------------------------------------------------------------------
#include <stdlib.h>  // atoi
#include "column/view.h"
#include "frame/py_frame.h"
#include "python/obj.h"
#include "python/string.h"
#include "python/pybuffer.h"
#include "utils/assert.h"
#include "utils/exceptions.h"
#include "column.h"
#include "datatablemodule.h"
#include "encodings.h"

namespace pybuffers {
  size_t single_col;
  SType force_stype;
}

// Forward declarations
static void try_to_resolve_object_column(Column& col);
static const char* format_from_stype(SType stype);
static Column convert_fwchararray_to_column(py::buffer&& view);

#define REQ_ND(flags)       ((flags & PyBUF_ND) == PyBUF_ND)
#define REQ_FORMAT(flags)   ((flags & PyBUF_FORMAT) == PyBUF_FORMAT)
#define REQ_STRIDES(flags)  ((flags & PyBUF_STRIDES) == PyBUF_STRIDES)
#define REQ_WRITABLE(flags) ((flags & PyBUF_WRITABLE) == PyBUF_WRITABLE)
// #define REQ_INDIRECT(flags) ((flags & PyBUF_INDIRECT) == PyBUF_INDIRECT)
// #define REQ_C_CONTIG(flags) ((flags & PyBUF_C_CONTIGUOUS) == PyBUF_C_CONTIGUOUS)
// #define REQ_F_CONTIG(flags) ((flags & PyBUF_F_CONTIGUOUS) == PyBUF_F_CONTIGUOUS)

static char strB[] = "B";




//------------------------------------------------------------------------------
// Construct a Column from a python object implementing Buffers protocol
//------------------------------------------------------------------------------

Column Column::from_pybuffer(const py::robj& pyobj)
{
  // Check if this is a datetime64 column, in which case it must be converted
  if (pyobj.is_numpy_array()) {
    auto dtype = pyobj.get_attr("dtype");
    auto kind = dtype.get_attr("kind").to_string();
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
  if (view.stype() == SType::STR32) {
    res = convert_fwchararray_to_column(std::move(view));
  }
  else {
    res = std::move(view).to_column();
  }

  if (res.stype() == SType::OBJ) {
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
    offset += bytes_len;
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
      out[i] = v == Py_True? 1 : v == Py_False? 0 : GETNA<int8_t>();
    }
    col = Column::new_mbuf_column(nrows, SType::BOOL, std::move(mbuf));
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
          strbuf_size = static_cast<size_t>(1.5 * strbuf_size + sz);
          strbuf.resize(strbuf_size);
          strs = static_cast<char*>(strbuf.xptr());
        }
        std::memcpy(strs + offset, PyBytes_AsString(z), sz);
        Py_DECREF(z);
        offset += sz;
        offsets[i] = offset;
      } else {
        offsets[i] = offset ^ GETNA<uint32_t>();
      }
    }

    xassert(offset < strbuf.size());
    strbuf.resize(offset);
    col = Column::new_string_column(nrows, std::move(offbuf), std::move(strbuf));
  }
}




//==============================================================================
// Buffers protocol for Python-side Frame object
//
// These methods are needed to support the buffers protocol for the Python-side
// `DataTable` object. We define these methods here, and then use
// `pyinstall_buffer_hooks` in order to attach this behavior to the PyType
// corresponding to the Python::DataTable.
//
// This is somewhat hacky, however there is no other way to implement the
// buffers protocol in pure Python. And we need to implement this protocol, so
// that DataTable objects can be passed to Numpy/Pandas, i.e. it supports the
// code like this:
//
//     >>> d = dt.DataTable(...)
//     >>> import numpy as np
//     >>> a = np.array(d)
//
//==============================================================================

struct XInfo {
  // Exported Buffer object.
  Buffer mbuf;

  // An array of Py_ssize_t of length `ndim`, indicating the shape of the
  // memory as an n-dimensional array (prod(shape) * itemsize == len).
  // Must be provided iff PyBUF_ND is set.
  Py_ssize_t shape[2];

  // An array of Py_ssize_t of length `ndim` giving the number of bytes to skip
  // to get to a new element in each dimension.
  // Must be provided iff PyBUF_STRIDES is set.
  Py_ssize_t strides[2];

  // Stype of the exported data
  SType stype;

  int64_t : 56;

  XInfo() {
    shape[0] = shape[1] = 0;
    strides[0] = strides[1] = 0;
    stype = SType::VOID;
    TRACK(this, sizeof(*this), "py-buffer");
  }

  ~XInfo() {
    UNTRACK(this);
  }
};



static void getbuffer_no_cols(py::Frame* self, Py_buffer* view, int flags)
{
  XInfo* xinfo = nullptr;
  xinfo = new XInfo();
  view->buf = nullptr;
  view->obj = py::oobj(self).release();
  view->len = 0;
  view->readonly = 0;
  view->itemsize = 1;
  view->format = REQ_FORMAT(flags) ? strB : nullptr;
  view->ndim = 2;
  view->shape = REQ_ND(flags) ? xinfo->shape : nullptr;
  view->strides = REQ_STRIDES(flags) ? xinfo->strides : nullptr;
  view->suboffsets = nullptr;
  view->internal = xinfo;
}


static void getbuffer_1_col(py::Frame* self, Py_buffer* view, int flags)
{
  bool one_col = (pybuffers::single_col != size_t(-1));
  size_t i0 = one_col? pybuffers::single_col : 0;
  XInfo* xinfo = nullptr;
  const Column& col = self->get_datatable()->get_column(i0);
  const char* fmt = format_from_stype(col.stype());

  xinfo = new XInfo();
  xinfo->mbuf = col.get_data_buffer();
  xinfo->shape[0] = static_cast<Py_ssize_t>(col.nrows());
  xinfo->shape[1] = 1;
  xinfo->strides[0] = static_cast<Py_ssize_t>(col.elemsize());
  xinfo->strides[1] = static_cast<Py_ssize_t>(xinfo->mbuf.size());
  xinfo->stype = col.stype();

  view->buf = const_cast<void*>(xinfo->mbuf.rptr());
  view->obj = py::oobj(self).release();
  view->len = xinfo->strides[1];
  view->readonly = 1;
  view->itemsize = xinfo->strides[0];
  view->format = REQ_FORMAT(flags) ? const_cast<char*>(fmt) : nullptr;
  view->ndim = one_col? 1 : 2;
  view->shape = REQ_ND(flags) ? xinfo->shape : nullptr;
  view->strides = REQ_STRIDES(flags) ? xinfo->strides : nullptr;
  view->suboffsets = nullptr;
  view->internal = xinfo;
}



void py::Frame::m__getbuffer__(Py_buffer* view, int flags) {
  XInfo* xinfo = nullptr;
  size_t ncols = dt->ncols();
  size_t nrows = dt->nrows();
  size_t i0 = 0;
  bool one_col = (pybuffers::single_col != size_t(-1));
  if (one_col) {
    ncols = 1;
    i0 = pybuffers::single_col;
  }

  if (ncols == 0) {
    return getbuffer_no_cols(this, view, flags);
  }

  // Check whether we have a single-column DataTable that doesn't need to be
  // copied -- in which case it should be possible to return the buffer
  // by-reference instead of copying the data into an intermediate buffer.
  const Column& col_i0 = dt->get_column(i0);
  if (ncols == 1 && !col_i0.is_virtual() && !REQ_WRITABLE(flags) &&
      col_i0.is_fixedwidth() && pybuffers::force_stype == SType::VOID)
  {
    return getbuffer_1_col(this, view, flags);
  }

  // Multiple columns datatable => copy all data into a new buffer before
  // passing it to the requester. This is of course very unfortunate, but
  // Numpy (the primary consumer of the buffer protocol) is unable to handle
  // "INDIRECT" buffer.

  // First, find the common stype for all columns in the DataTable.
  SType stype = pybuffers::force_stype;
  if (stype == SType::VOID) {
    // Auto-detect common stype
    for (size_t i = 0; i < ncols; ++i) {
      SType next_stype = dt->get_column(i + i0).stype();
      stype = common_stype(stype, next_stype);
    }
  }
  if (stype == SType::INVALID ||
      stype == SType::STR32 ||
      stype == SType::STR64) stype = SType::OBJ;

  // Allocate the final buffer
  xassert(!info(stype).is_varwidth());
  size_t elemsize = info(stype).elemsize();
  size_t colsize = nrows * elemsize;
  Buffer memr = Buffer::mem(ncols * colsize);
  const char* fmt = format_from_stype(stype);

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
      Column newcol = dt->get_column(i + i0).cast(stype, std::move(xmb));
      newcol.materialize();
      // We can now delete the new column: this will delete `xmb` as well,
      // however a "view buffer" object does not attempt to free its
      // memory. The converted data that was written to `xmb` will
      // thus remain intact.
    }
  }
  if (stype == SType::OBJ) {
    memr.set_pyobjects(/*clear=*/ false);
  }

  xinfo = new XInfo();
  xinfo->mbuf = std::move(memr);
  xinfo->shape[0] = static_cast<Py_ssize_t>(nrows);
  xinfo->shape[1] = static_cast<Py_ssize_t>(ncols);
  xinfo->strides[0] = static_cast<Py_ssize_t>(elemsize);
  xinfo->strides[1] = static_cast<Py_ssize_t>(colsize);
  xinfo->stype = stype;

  // Fill in the `view` struct
  view->buf = const_cast<void*>(xinfo->mbuf.rptr());
  view->obj = py::oobj(this).release();
  view->len = static_cast<Py_ssize_t>(xinfo->mbuf.size());
  view->readonly = 0;
  view->itemsize = static_cast<Py_ssize_t>(elemsize);
  view->format = REQ_FORMAT(flags) ? const_cast<char*>(fmt) : nullptr;
  view->ndim = one_col? 1 : 2;
  view->shape = REQ_ND(flags)? xinfo->shape : nullptr;
  view->strides = REQ_STRIDES(flags)? xinfo->strides : nullptr;
  view->suboffsets = nullptr;
  view->internal = xinfo;
}

void py::Frame::m__releasebuffer__(Py_buffer* view) {
  XInfo* xinfo = static_cast<XInfo*>(view->internal);
  delete xinfo;
}



static py::PKArgs args__install_buffer_hooks(
  1, 0, 0, false, false, {"obj"}, "_install_buffer_hooks", nullptr);

static void _install_buffer_hooks(const py::PKArgs& args)
{
  PyObject* obj = args[0].to_borrowed_ref();
  if (obj) {
    auto frame_type = reinterpret_cast<PyObject*>(&py::Frame::type);
    int ret = PyObject_IsSubclass(obj, frame_type);
    if (ret == -1) throw PyError();
    if (ret == 0) {
      throw ValueError() << "Function `_install_buffer_hooks()` can only be "
          "applied to subclasses of core.Frame";
    }
    auto type = reinterpret_cast<PyTypeObject*>(obj);
    type->tp_as_buffer = py::Frame::type.tp_as_buffer;
  }
}


void py::DatatableModule::init_methods_buffers() {
  ADD_FN(&_install_buffer_hooks, args__install_buffer_hooks);
  pybuffers::single_col = size_t(-1);
  pybuffers::force_stype = SType::VOID;
}




//==============================================================================
// Buffers utility functions
//==============================================================================

static const char* format_from_stype(SType stype)
{
  return stype == SType::BOOL? "?" :
         stype == SType::INT8? "b" :
         stype == SType::INT16? "h" :
         stype == SType::INT32? "i" :
         stype == SType::INT64? "q" :
         stype == SType::FLOAT32? "f" :
         stype == SType::FLOAT64? "d" :
         stype == SType::OBJ? "O" : "x";
}
