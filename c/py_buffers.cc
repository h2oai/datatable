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
#include "python/obj.h"
#include "utils/assert.h"
#include "utils/exceptions.h"
#include "column.h"
#include "datatablemodule.h"
#include "encodings.h"
#include "py_column.h"
#include "py_datatable.h"
#include "py_types.h"
#include "py_utils.h"

SType force_stype;

// Forward declarations
static Column* try_to_resolve_object_column(Column* col);
static SType stype_from_format(const char *format, int64_t itemsize);
static const char* format_from_stype(SType stype);
static Column* convert_fwchararray_to_column(Py_buffer* view);

#define REQ_ND(flags)       ((flags & PyBUF_ND) == PyBUF_ND)
#define REQ_FORMAT(flags)   ((flags & PyBUF_FORMAT) == PyBUF_FORMAT)
#define REQ_STRIDES(flags)  ((flags & PyBUF_STRIDES) == PyBUF_STRIDES)
#define REQ_WRITABLE(flags) ((flags & PyBUF_WRITABLE) == PyBUF_WRITABLE)
// #define REQ_INDIRECT(flags) ((flags & PyBUF_INDIRECT) == PyBUF_INDIRECT)
// #define REQ_C_CONTIG(flags) ((flags & PyBUF_C_CONTIGUOUS) == PyBUF_C_CONTIGUOUS)
// #define REQ_F_CONTIG(flags) ((flags & PyBUF_F_CONTIGUOUS) == PyBUF_F_CONTIGUOUS)

static char strB[] = "B";

#define DECLARE_INIT_BUFFER(BASECLS, CLSNAME)                                  \
  static int getbuffer_ ## CLSNAME(BASECLS*, Py_buffer*, int);                 \
  ES_INT_FUNCTION(                                                             \
    static int safe_getbuffer_##CLSNAME(                                       \
      BASECLS* self, Py_buffer* buf, int flags),                               \
    getbuffer_ ## CLSNAME(self, buf, flags),                                   \
    STRINGIFY(CLSNAME) "::get_buffer()")

#define DECLARE_RELEASE_BUFFER(BASECLS, CLSNAME)                               \
  static void releasebuffer_ ## CLSNAME(BASECLS*, Py_buffer*);                 \
  ES_VOID_FUNCTION(                                                            \
    static void safe_releasebuffer_ ## CLSNAME(BASECLS* self, Py_buffer* buf), \
    releasebuffer_ ## CLSNAME(self, buf),                                      \
    STRINGIFY(CLSNAME) "::release_buffer()")

#define DECLARE_BUFFERS_STRUCT(STRUCTNAME, CLSNAME)                            \
  PyBufferProcs STRUCTNAME = {                                                 \
    reinterpret_cast<getbufferproc>(safe_getbuffer_##CLSNAME),                 \
    reinterpret_cast<releasebufferproc>(safe_releasebuffer_##CLSNAME),         \
  };



//------------------------------------------------------------------------------
// Construct a DataTable from a list of objects implementing Buffers protocol
//------------------------------------------------------------------------------

Column* Column::from_buffer(const py::robj& obuffer)
{
  PyObject* buffer = obuffer.to_borrowed_ref();
  Py_buffer* view = static_cast<Py_buffer*>(std::calloc(1, sizeof(Py_buffer)));
  if (!view) throw PyError();

  // Request the buffer (not writeable). Flag PyBUF_FORMAT indicates that
  // the `view->format` field should be filled; and PyBUF_ND will fill the
  // `view->shape` information (while `strides` and `suboffsets` will be
  // nullptr).
  int ret = PyObject_GetBuffer(buffer, view, PyBUF_FORMAT | PyBUF_ND);
  if (ret != 0) {
    PyErr_Clear();  // otherwise system functions may fail later on

    // Check if this is a datetime64 column, in which case it must be converted
    if (obuffer.is_numpy_array()) {
      auto kind = obuffer.get_attr("dtype").get_attr("kind").to_string();
      if (kind == "M" || kind == "m") {
        return Column::from_buffer(obuffer.invoke("astype", "(s)", "str"));
      }
    }

    ret = PyObject_GetBuffer(buffer, view, PyBUF_FORMAT | PyBUF_STRIDES);
    if (ret != 0) throw PyError();
  }
  if (view->ndim != 1) {
    throw NotImplError() << "Source buffer has ndim=" << view->ndim
      << ", however only 1-D buffers are supported";
  }

  // If buffer is in float16 format, convert it to float32
  if (view->itemsize == 2 && std::strcmp(view->format, "e") == 0) {
    PyBuffer_Release(view);
    py::oobj newbuf = py::robj(buffer).invoke("astype", "(s)", "float32");
    return Column::from_buffer(newbuf);
  }

  SType stype = stype_from_format(view->format, view->itemsize);
  size_t nrows = static_cast<size_t>(view->len / view->itemsize);

  Column* res = nullptr;
  if (stype == SType::STR32) {
    res = convert_fwchararray_to_column(view);
  } else if (view->strides == nullptr) {
    res = Column::new_xbuf_column(stype, nrows, view);
  } else {
    res = Column::new_data_column(stype, nrows);
    size_t stride = static_cast<size_t>(view->strides[0] / view->itemsize);
    if (view->itemsize == 8) {
      int64_t* out = static_cast<int64_t*>(res->data_w());
      int64_t* inp = static_cast<int64_t*>(view->buf);
      for (size_t j = 0; j < nrows; ++j) {
        out[j] = inp[j * stride];
      }
    } else if (view->itemsize == 4) {
      int32_t* out = static_cast<int32_t*>(res->data_w());
      int32_t* inp = static_cast<int32_t*>(view->buf);
      for (size_t j = 0; j < nrows; ++j) {
        out[j] = inp[j * stride];
      }
    } else if (view->itemsize == 2) {
      int16_t* out = static_cast<int16_t*>(res->data_w());
      int16_t* inp = static_cast<int16_t*>(view->buf);
      for (size_t j = 0; j < nrows; ++j) {
        out[j] = inp[j * stride];
      }
    } else if (view->itemsize == 1) {
      int8_t* out = static_cast<int8_t*>(res->data_w());
      int8_t* inp = static_cast<int8_t*>(view->buf);
      for (size_t j = 0; j < nrows; ++j) {
        out[j] = inp[j * stride];
      }
    }
  }
  if (res->stype() == SType::OBJ) {
    res = try_to_resolve_object_column(res);
  }
  return res;
}


static Column* convert_fwchararray_to_column(Py_buffer* view)
{
  // Number of characters in each element
  size_t k = static_cast<size_t>(view->itemsize / 4);
  size_t nrows = static_cast<size_t>(view->len / view->itemsize);
  size_t stride = view->strides? static_cast<size_t>(view->strides[0])/4 : k;
  size_t maxsize = static_cast<size_t>(view->len);
  auto input = reinterpret_cast<uint32_t*>(view->buf);

  MemoryRange strbuf = MemoryRange::mem(maxsize);
  MemoryRange offbuf = MemoryRange::mem((nrows + 1) * 4);
  char* strptr = static_cast<char*>(strbuf.wptr());
  uint32_t* offptr = static_cast<uint32_t*>(offbuf.wptr());
  *offptr++ = 0;
  uint32_t offset = 0;
  for (size_t j = 0; j < nrows; ++j) {
    uint32_t* start = input + j*stride;
    int64_t bytes_len = utf32_to_utf8(start, k, strptr);
    offset += bytes_len;
    strptr += bytes_len;
    *offptr++ = offset;
  }

  strbuf.resize(static_cast<size_t>(offset));
  return new StringColumn<uint32_t>(nrows,
                                    std::move(offbuf), std::move(strbuf));
}



/**
 * If a column was created from Pandas series, it may end up having
 * dtype='object' (because Pandas doesn't support string columns). This function
 * will attempt to convert such column to the string type (or some other type
 * if more appropriate), and return either the original or the new modified
 * column. If a new column is returned, the original one is decrefed.
 */
static Column* try_to_resolve_object_column(Column* col)
{
  PyObject* const* data = static_cast<PyObject* const*>(col->data());
  size_t nrows = col->nrows;

  int all_strings = 1;
  // Approximate total length of all strings. Do not take into account
  // possibility that the strings may expand in UTF-8 -- if needed, we'll
  // realloc the buffer later.
  size_t total_length = 10;
  for (size_t i = 0; i < nrows; ++i) {
    if (data[i] == Py_None) continue;
    if (!PyUnicode_Check(data[i])) {
      all_strings = 0;
      break;
    }
    total_length += static_cast<size_t>(PyUnicode_GetLength(data[i]));
  }

  // Not all elements were strings -- return the original column unmodified
  if (!all_strings) {
    return col;
  }

  // Otherwise the column is all-strings: convert it into *STRING stype.
  size_t strbuf_size = total_length;
  MemoryRange offbuf = MemoryRange::mem((nrows + 1) * 4);
  MemoryRange strbuf = MemoryRange::mem(strbuf_size);
  uint32_t* offsets = static_cast<uint32_t*>(offbuf.xptr());
  char* strs = static_cast<char*>(strbuf.xptr());

  offsets[0] = 0;
  ++offsets;

  uint32_t offset = 0;
  for (size_t i = 0; i < nrows; ++i) {
    if (data[i] == Py_None) {
      offsets[i] = offset | GETNA<uint32_t>();
    } else {
      PyObject *z = PyUnicode_AsEncodedString(data[i], "utf-8", "strict");
      size_t sz = static_cast<size_t>(PyBytes_Size(z));
      if (offset + sz > strbuf_size) {
        strbuf_size = static_cast<size_t>(1.5 * strbuf_size + sz);
        strbuf.resize(strbuf_size);
        strs = static_cast<char*>(strbuf.xptr()); // Location may have changed
      }
      std::memcpy(strs + offset, PyBytes_AsString(z), sz);
      Py_DECREF(z);
      offset += sz;
      offsets[i] = offset;
    }
  }

  xassert(offset < strbuf.size());
  strbuf.resize(offset);
  delete col;
  return new StringColumn<uint32_t>(nrows, std::move(offbuf), std::move(strbuf));
}



//==============================================================================
// Buffers interface for pycolumn::obj
//==============================================================================

struct XInfo {
  // Exported MemoryRange object.
  MemoryRange mbuf;

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


/**
 * Handle a request to fill-in structure `view` as specified by `flags`.
 * See https://docs.python.org/3/c-api/typeobj.html#buffer-structs for details.
 *
 * This function:
 *   - increments refcount and stores pointer to `self` in `view->obj`;
 *   - allocates `XInfo` structure and stores it in the `view->internal`;
 *   - creates a shallow copy of the Column (together with its buffer), and
 *     stores it in the `XInfo` struct.
 */
static int getbuffer_Column(pycolumn::obj* self, Py_buffer* view, int flags) {
  XInfo* xinfo = nullptr;
  Column* col = self->ref;

  if (REQ_WRITABLE(flags)) {
    // Do not provide a writable array (this may violate all kinds of internal
    // assumptions about the data). Instead let the requester ask again, this
    // time for a read-only buffer.
    // Do not use a C++ exception here, as it may create deadlocks on rare
    // occasions.
    PyErr_SetString(PyExc_ValueError, "Cannot create a writable buffer");
    return -1;
  }
  if (info(col->stype()).is_varwidth()) {
    throw ValueError() << "Column's data has variable width";
  }

  xinfo = new XInfo();
  xinfo->mbuf = col->data_buf();
  xinfo->shape[0] = static_cast<Py_ssize_t>(col->nrows);
  xinfo->strides[0] = static_cast<Py_ssize_t>(col->elemsize());
  xinfo->stype = col->stype();

  view->buf = const_cast<void*>(xinfo->mbuf.rptr());
  view->obj = reinterpret_cast<PyObject*>(self);
  view->len = static_cast<Py_ssize_t>(xinfo->mbuf.size());
  view->itemsize = xinfo->strides[0];
  view->readonly = 1;
  view->ndim = 1;
  view->shape = REQ_ND(flags)? xinfo->shape : nullptr;
  view->strides = REQ_STRIDES(flags)? xinfo->strides : nullptr;
  view->suboffsets = nullptr;
  view->internal = xinfo;
  view->format = REQ_FORMAT(flags) ?
      const_cast<char*>(format_from_stype(col->stype())) : nullptr;

  Py_INCREF(self);
  return 0;
}


/**
 * Handle a request to release the resources of the buffer.
 *
 * This function MUST NOT decrement view->obj (== self), since it is done by
 * Python in `PyBuffer_Release()`.
 */
static void releasebuffer_Column(pycolumn::obj*, Py_buffer* view) {
  XInfo* xinfo = static_cast<XInfo*>(view->internal);
  delete xinfo;
  view->internal = nullptr;
}


DECLARE_INIT_BUFFER(pycolumn::obj, Column)
DECLARE_RELEASE_BUFFER(pycolumn::obj, Column)
DECLARE_BUFFERS_STRUCT(pycolumn::as_buffer, Column);



//==============================================================================
// Buffers interface for pydatatable::obj
//==============================================================================

static int dt_getbuffer_no_cols(
  pydatatable::obj* self, Py_buffer* view, int flags)
{
  XInfo* xinfo = nullptr;
  xinfo = new XInfo();
  view->buf = nullptr;
  view->obj = incref(reinterpret_cast<PyObject*>(self));
  view->len = 0;
  view->readonly = 0;
  view->itemsize = 1;
  view->format = REQ_FORMAT(flags) ? strB : nullptr;
  view->ndim = 2;
  view->shape = REQ_ND(flags) ? xinfo->shape : nullptr;
  view->strides = REQ_STRIDES(flags) ? xinfo->strides : nullptr;
  view->suboffsets = nullptr;
  view->internal = xinfo;
  return 0;
}


static int dt_getbuffer_1_col(
  pydatatable::obj* self, Py_buffer* view, int flags)
{
  XInfo* xinfo = nullptr;
  Column* col = self->ref->columns[0];
  const char* fmt = format_from_stype(col->stype());

  xinfo = new XInfo();
  xinfo->mbuf = col->data_buf();
  xinfo->shape[0] = static_cast<Py_ssize_t>(col->nrows);
  xinfo->shape[1] = 1;
  xinfo->strides[0] = static_cast<Py_ssize_t>(col->elemsize());
  xinfo->strides[1] = static_cast<Py_ssize_t>(xinfo->mbuf.size());
  xinfo->stype = col->stype();

  view->buf = const_cast<void*>(xinfo->mbuf.rptr());
  view->obj = incref(reinterpret_cast<PyObject*>(self));
  view->len = xinfo->strides[1];
  view->readonly = 1;
  view->itemsize = xinfo->strides[0];
  view->format = REQ_FORMAT(flags) ? const_cast<char*>(fmt) : nullptr;
  view->ndim = 2;
  view->shape = REQ_ND(flags) ? xinfo->shape : nullptr;
  view->strides = REQ_STRIDES(flags) ? xinfo->strides : nullptr;
  view->suboffsets = nullptr;
  view->internal = xinfo;
  return 0;
}


static int getbuffer_DataTable(
  pydatatable::obj* self, Py_buffer* view, int flags)
{
  XInfo* xinfo = nullptr;
  DataTable* dt = self->ref;
  size_t ncols = dt->ncols;
  size_t nrows = dt->nrows;

  if (ncols == 0) {
    return dt_getbuffer_no_cols(self, view, flags);
  }

  // Check whether we have a single-column DataTable that doesn't need to be
  // copied -- in which case it should be possible to return the buffer
  // by-reference instead of copying the data into an intermediate buffer.
  if (ncols == 1 && !dt->columns[0]->rowindex() && !REQ_WRITABLE(flags) &&
      dt->columns[0]->is_fixedwidth() && force_stype == SType::VOID) {
    return dt_getbuffer_1_col(self, view, flags);
  }

  // Multiple columns datatable => copy all data into a new buffer before
  // passing it to the requester. This is of course very unfortunate, but
  // Numpy (the primary consumer of the buffer protocol) is unable to handle
  // "INDIRECT" buffer.

  // First, find the common stype for all columns in the DataTable.
  SType stype = force_stype;
  if (stype == SType::VOID) {
    // Auto-detect common stype
    uint64_t stypes_mask = 0;
    for (size_t i = 0; i < ncols; ++i) {
      SType next_stype = dt->columns[i]->stype();
      uint64_t unstype = static_cast<uint64_t>(next_stype);
      if (stypes_mask & (1 << unstype)) continue;
      stypes_mask |= 1 << unstype;
      stype = common_stype_for_buffer(stype, next_stype);
    }
  }

  // Allocate the final buffer
  xassert(!info(stype).is_varwidth());
  size_t elemsize = info(stype).elemsize();
  size_t colsize = nrows * elemsize;
  MemoryRange memr = MemoryRange::mem(ncols * colsize);
  const char* fmt = format_from_stype(stype);

  // Construct the data buffer
  for (size_t i = 0; i < ncols; ++i) {
    // either a shallow copy, or "materialized" column
    Column* col = dt->columns[i]->shallowcopy();
    col->reify();
    if (col->stype() == stype) {
      xassert(col->alloc_size() == colsize);
      std::memcpy(memr.wptr(i*colsize), col->data(), colsize);
    } else {
      // xmb becomes a "view" on a portion of the buffer `mbuf`. An
      // ExternelMemBuf object is documented to be readonly; however in
      // practice it can still be written to, just not resized (this is
      // hacky, maybe fix in the future).
      MemoryRange xmb = MemoryRange::view(memr, colsize, i*colsize);
      // Now we create a `newcol` by casting `col` into `stype`, using
      // the buffer `xmb`. Since `xmb` already has the correct size, this
      // is possible. The effect of this call is that `newcol` will be
      // created having the converted data; but the side-effect of this is
      // that `mbuf` will have the same data, and in the right place.
      Column* newcol = col->cast(stype, std::move(xmb));
      xassert(newcol->alloc_size() == colsize);
      // We can now delete the new column: this will delete `xmb` as well,
      // however an ExternalMemBuf object does not attempt to free its
      // memory buffer. The converted data that was written to `mbuf` will
      // thus remain intact. No need to delete `xmb` either.
      delete newcol;
    }
    // Delete the `col` pointer, which was extracted from the i-th column
    // of the DataTable.
    delete col;
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
  view->obj = incref(reinterpret_cast<PyObject*>(self));
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
}


/*
    There is a bug in Numpy where they request for an "indirect" buffer but then
    do not know how to handle it... In order to circumvent that we will always
    create "strides" buffer.
    https://github.com/numpy/numpy/issues/9456

    if (REQ_INDIRECT(flags)) {
        size_t elemsize = info(stype).elemsize();
        Py_ssize_t *info = dt::amalloc<Py_ssize_t>(6);
        void** buf = dt::amalloc<void*>(ncols);
        for (int i = 0; i < ncols; i++) {
            dt->columns[i]->incref();
            buf[i] = dt->columns[i]->data;
        }

        view->buf = (void*) buf;
        view->obj = (PyObject*) self;
        view->len = ncols * dt->nrows * elemsize;
        view->readonly = 1;
        view->itemsize = elemsize;
        view->format = REQ_FORMAT(flags)? format_from_stype(stype) : nullptr;
        view->ndim = 2;
        info[0] = ncols;
        info[1] = dt->nrows;
        view->shape = info;
        info[2] = sizeof(void*);
        info[3] = elemsize;
        view->strides = info + 2;
        info[4] = 0;
        info[5] = -1;
        view->suboffsets = info + 4;
        view->internal = (void*) 1;

        Py_INCREF(self);
        return 0;
    }
*/

static void releasebuffer_DataTable(pydatatable::obj*, Py_buffer *view) {
  XInfo* xinfo = static_cast<XInfo*>(view->internal);
  delete xinfo;
}


DECLARE_INIT_BUFFER(pydatatable::obj, DataTable)
DECLARE_RELEASE_BUFFER(pydatatable::obj, DataTable)
DECLARE_BUFFERS_STRUCT(pydatatable::as_buffer, DataTable);



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

static int getbuffer_Frame(PyObject* self, Py_buffer* view, int flags)
{
  PyObject* pydt = PyObject_GetAttrString(self, "internal");
  if (pydt == nullptr) {
    PyErr_Format(PyExc_AttributeError,
                 "Cannot retrieve attribute internal");
    return -1;
  }
  pydatatable::obj* dt = reinterpret_cast<pydatatable::obj*>(pydt);
  return getbuffer_DataTable(dt, view, flags);
}

static void releasebuffer_Frame(PyObject*, Py_buffer* view)
{
  releasebuffer_DataTable(nullptr, view);
}


DECLARE_INIT_BUFFER(PyObject, Frame)
DECLARE_RELEASE_BUFFER(PyObject, Frame)
DECLARE_BUFFERS_STRUCT(static python_frame_as_buffer, Frame);


static py::PKArgs args__install_buffer_hooks(
  1, 0, 0, false, false, {"obj"}, "_install_buffer_hooks", nullptr);

static void _install_buffer_hooks(const py::PKArgs& args)
{
  PyObject* obj = args[0].to_borrowed_ref();
  if (obj) obj->ob_type->tp_as_buffer = &python_frame_as_buffer;
}


void DatatableModule::init_methods_buffers() {
  ADD_FN(&_install_buffer_hooks, args__install_buffer_hooks);
}




//==============================================================================
// Buffers utility functions
//==============================================================================

static SType stype_from_format(const char* format, int64_t itemsize)
{
  SType stype = SType::VOID;
  char c = format[0];
  if (c == '@' || c == '=') c = format[1];

  if (c == 'b' || c == 'h' || c == 'i' || c == 'l' || c == 'q' || c == 'n') {
    // These are all various integer types
    stype = itemsize == 1 ? SType::INT8 :
            itemsize == 2 ? SType::INT16 :
            itemsize == 4 ? SType::INT32 :
            itemsize == 8 ? SType::INT64 : SType::VOID;
  }
  else if (c == 'd' || c == 'f') {
    stype = itemsize == 4 ? SType::FLOAT32 :
            itemsize == 8 ? SType::FLOAT64 : SType::VOID;
  }
  else if (c == '?') {
    stype = itemsize == 1 ? SType::BOOL : SType::VOID;
  }
  else if (c == 'O') {
    stype = SType::OBJ;
  }
  else if (c >= '1' && c <= '9') {
    if (format[strlen(format) - 1] == 'w') {
      int numeral = atoi(format);
      if (itemsize == numeral * 4) {
        stype = SType::STR32;
      }
    }
  }
  if (stype == SType::VOID) {
    throw ValueError()
        << "Unknown format '" << format << "' with itemsize " << itemsize;
  }
  return stype;
}

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
