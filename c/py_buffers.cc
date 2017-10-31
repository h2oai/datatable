//------------------------------------------------------------------------------
//  Copyright 2017 H2O.ai
//
//  Licensed under the Apache License, Version 2.0 (the "License");
//  you may not use this file except in compliance with the License.
//  You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
//  Unless required by applicable law or agreed to in writing, software
//  distributed under the License is distributed on an "AS IS" BASIS,
//  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//  See the License for the specific language governing permissions and
//  limitations under the License.
//------------------------------------------------------------------------------
// Functionality related to "Buffers" interface
// See https://www.python.org/dev/peps/pep-3118/
//------------------------------------------------------------------------------
#include "column.h"
#include "py_column.h"
#include "py_datatable.h"
#include "py_types.h"
#include "py_utils.h"

// Forward declarations
Column* try_to_resolve_object_column(Column* col);
static SType stype_from_format(const char *format, int64_t itemsize);
static const char* format_from_stype(SType stype);

#define REQ_ND(flags)       ((flags & PyBUF_ND) == PyBUF_ND)
#define REQ_FORMAT(flags)   ((flags & PyBUF_FORMAT) == PyBUF_FORMAT)
#define REQ_STRIDES(flags)  ((flags & PyBUF_STRIDES) == PyBUF_STRIDES)
#define REQ_WRITABLE(flags) ((flags & PyBUF_WRITABLE) == PyBUF_WRITABLE)
// #define REQ_INDIRECT(flags) ((flags & PyBUF_INDIRECT) == PyBUF_INDIRECT)
// #define REQ_C_CONTIG(flags) ((flags & PyBUF_C_CONTIGUOUS) == PyBUF_C_CONTIGUOUS)
// #define REQ_F_CONTIG(flags) ((flags & PyBUF_F_CONTIGUOUS) == PyBUF_F_CONTIGUOUS)

static char strB[] = "B";


/**
 * Load datatable from a list of Python objects supporting Buffers protocol.
 *
 * See: https://docs.python.org/3/c-api/buffer.html
 */
PyObject* pydatatable_from_buffers(PyObject*, PyObject* args)
{
  try {
    PyObject* list = nullptr;
    if (!PyArg_ParseTuple(args, "O!:from_buffers", &PyList_Type, &list))
      return nullptr;

    int n = static_cast<int>(PyList_Size(list));
    Column** columns = nullptr;
    dtmalloc(columns, Column*, n + 1);
    columns[n] = nullptr;

    for (int i = 0; i < n; ++i) {
      PyObject *item = PyList_GET_ITEM(list, i);
      if (!PyObject_CheckBuffer(item)) {
        throw ValueError() << "Element " << i << " in the list of sources "
                           << "does not support buffers interface";
      }
      Py_buffer* view;
      dtcalloc(view, Py_buffer, 1);

      // Request the buffer (not writeable). Flag PyBUF_FORMAT indicates that
      // the `view->format` field should be filled; and PyBUF_ND will fill the
      // `view->shape` information (while `strides` and `suboffsets` will be
      // nullptr).
      int ret = PyObject_GetBuffer(item, view, PyBUF_FORMAT | PyBUF_ND);
      if (ret != 0) {
        PyErr_Clear();  // otherwise system functions may fail later on
        ret = PyObject_GetBuffer(item, view, PyBUF_FORMAT | PyBUF_STRIDES);
      }
      if (ret != 0) {
        if (PyErr_Occurred()) {
          throw PyError();
        } else {
          throw RuntimeError() << "Unable to retrieve buffer for column " << i;
        }
      }
      if (view->ndim != 1) {
        throw NotImplError() << "Buffer has ndim=" << view->ndim
                             << ", cannot handle";
      }

      SType stype = stype_from_format(view->format, view->itemsize);
      int64_t nrows = view->len / view->itemsize;
      if (stype == ST_VOID) return nullptr;
      if (view->strides == nullptr) {
        columns[i] = Column::new_xbuf_column(stype, nrows, view, view->buf,
                                             static_cast<size_t>(view->len));
      } else {
        columns[i] = Column::new_data_column(stype, nrows);
        int64_t stride = view->strides[0] / view->itemsize;
        if (view->itemsize == 8) {
          int64_t* out = reinterpret_cast<int64_t*>(columns[i]->data());
          int64_t* inp = reinterpret_cast<int64_t*>(view->buf);
          for (int64_t j = 0; j < nrows; ++j)
            out[j] = inp[j * stride];
        } else if (view->itemsize == 4) {
          int32_t* out = reinterpret_cast<int32_t*>(columns[i]->data());
          int32_t* inp = reinterpret_cast<int32_t*>(view->buf);
          for (int64_t j = 0; j < nrows; ++j)
            out[j] = inp[j * stride];
        } else if (view->itemsize == 2) {
          int16_t* out = reinterpret_cast<int16_t*>(columns[i]->data());
          int16_t* inp = reinterpret_cast<int16_t*>(view->buf);
          for (int64_t j = 0; j < nrows; ++j)
            out[j] = inp[j * stride];
        } else if (view->itemsize == 1) {
          int8_t* out = reinterpret_cast<int8_t*>(columns[i]->data());
          int8_t* inp = reinterpret_cast<int8_t*>(view->buf);
          for (int64_t j = 0; j < nrows; ++j)
            out[j] = inp[j * stride];
        }
      }
      if (columns[i]->stype() == ST_OBJECT_PYPTR) {
        columns[i] = try_to_resolve_object_column(columns[i]);
      }
    }

    DataTable *dt = new DataTable(columns);
    return pydt_from_dt(dt);

  } catch (const std::exception& e) {
    PyErr_SetString(PyExc_RuntimeError, e.what());
    return NULL;
  }
}


/**
 * If a column was created from Pandas series, it may end up having
 * dtype='object' (because Pandas doesn't support string columns). This function
 * will attempt to convert such column to the string type (or some other type
 * if more appropriate), and return either the original or the new modified
 * column. If a new column is returned, the original one is decrefed.
 */
Column* try_to_resolve_object_column(Column* col)
{
  PyObject **data = (PyObject**) col->data();
  int64_t nrows = col->nrows;

  int all_strings = 1;
  // Approximate total length of all strings. Do not take into account
  // possibility that the strings may expand in UTF-8 -- if needed, we'll
  // realloc the buffer later.
  int64_t total_length = 10;
  for (int64_t i = 0; i < nrows; ++i) {
    if (data[i] == Py_None) continue;
    if (!PyUnicode_Check(data[i])) {
      all_strings = 0;
      break;
    }
    total_length += PyUnicode_GetLength(data[i]);
  }

  // Not all elements were strings -- return the original column unmodified
  if (!all_strings) {
    return col;
  }

  // Otherwise the column is all-strings: convert it into *STRING stype.
  char *strbuf = NULL;
  dtmalloc(strbuf, char, total_length);
  size_t strbuf_size = static_cast<size_t>(total_length);
  StringColumn<int32_t>* res = new StringColumn<int32_t>(nrows);
  int32_t* offsets = res->offsets();

  size_t offset = 0;
  for (int64_t i = 0; i < nrows; i++) {
    if (data[i] == Py_None) {
      offsets[i] = static_cast<int32_t>(-offset - 1);
    } else {
      PyObject *z = PyUnicode_AsEncodedString(data[i], "utf-8", "strict");
      size_t sz = static_cast<size_t>(PyBytes_Size(z));
      if (offset + sz > strbuf_size) {
        strbuf_size = static_cast<size_t>(1.5 * strbuf_size);
        dtrealloc(strbuf, char, strbuf_size);
      }
      memcpy(strbuf + offset, PyBytes_AsString(z), sz);
      Py_DECREF(z);
      offset += sz;
      offsets[i] = static_cast<int32_t>(offset + 1);
    }
  }

  size_t datasize = offset;
  size_t padding = Column::i4s_padding(datasize);
  size_t allocsize = datasize + padding + 4 * (size_t)nrows;
  dtrealloc(strbuf, char, allocsize);
  memset(strbuf + datasize, 0xFF, padding);
  memcpy(strbuf + datasize + padding, offsets, 4 * (size_t)nrows);
  res->mbuf = new MemoryMemBuf(static_cast<void*>(strbuf), allocsize);
  res->offoff = static_cast<int32_t>(datasize + padding);
  delete col;
  return res;
}


//==============================================================================
// Buffers interface for pycolumn::obj
//==============================================================================

typedef struct XInfo {
  // Shallow copy of the exported MemoryBuffer.
  MemoryBuffer* mbuf;

  // An array of Py_ssize_t of length `ndim`, indicating the shape of the
  // memory as an n-dimensional array (prod(shape) * itemsize == len).
  // Must be provided iff PyBUF_ND is set.
  Py_ssize_t shape[2];

  // An array of Py_ssize_t of length `ndim` giving the number of bytes to skip
  // to get to a new element in each dimension.
  // Must be provided iff PyBUF_STRIDES is set.
  Py_ssize_t strides[2];
} XInfo;


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
static int column_getbuffer(pycolumn::obj* self, Py_buffer* view, int flags)
{
  XInfo* xinfo = nullptr;
  MemoryBuffer* mbuf = nullptr;
  try {
    Column* col = self->ref;
    mbuf = col->mbuf_shallowcopy();

    if (REQ_WRITABLE(flags)) {
      // Do not provide a writable array (this may violate all kinds of internal
      // assumptions about the data). Instead let the requester ask again, this
      // time for a read-only buffer.
      throw ValueError() << "Cannot create a writable buffer for a Column";
    }
    if (stype_info[col->stype()].varwidth) {
      throw ValueError() << "Column's data has variable width";
    }

    xinfo = new XInfo();
    xinfo->mbuf = mbuf;
    xinfo->shape[0] = static_cast<Py_ssize_t>(col->nrows);
    xinfo->strides[0] = static_cast<Py_ssize_t>(col->elemsize());

    view->buf = mbuf->get();
    view->obj = reinterpret_cast<PyObject*>(self);
    view->len = static_cast<Py_ssize_t>(mbuf->size());
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

  } catch (const std::exception& e) {
    view->obj = nullptr;
    view->internal = nullptr;
    delete xinfo;
    if (mbuf) mbuf->release();
    PyErr_SetString(PyExc_RuntimeError, e.what());
    return -1;
  }
}


/**
 * Handle a request to release the resources of the buffer.
 *
 * This function MUST NOT decrement view->obj (== self), since it is done by
 * Python in `PyBuffer_Release()`.
 */
static void column_releasebuffer(pycolumn::obj*, Py_buffer* view)
{
  XInfo* xinfo = static_cast<XInfo*>(view->internal);
  xinfo->mbuf->release();
  delete xinfo;
  view->internal = nullptr;
}


PyBufferProcs pycolumn::as_buffer = {
  (getbufferproc) column_getbuffer,
  (releasebufferproc) column_releasebuffer,
};




//==============================================================================
// Buffers interface for DataTable_PyObject
//==============================================================================

static int dt_getbuffer_no_cols(DataTable_PyObject *self, Py_buffer *view,
                                int flags)
{
  XInfo* xinfo = nullptr;
  try {
    xinfo = new XInfo();
    xinfo->mbuf = nullptr;
    xinfo->shape[0] = 0;
    xinfo->shape[1] = 0;
    xinfo->strides[0] = 0;
    xinfo->strides[1] = 0;

    view->buf = NULL;
    view->obj = incref(reinterpret_cast<PyObject*>(self));
    view->len = 0;
    view->readonly = 0;
    view->itemsize = 1;
    view->format = REQ_FORMAT(flags) ? strB : NULL;
    view->ndim = 2;
    view->shape = REQ_ND(flags) ? xinfo->shape : NULL;
    view->strides = REQ_STRIDES(flags) ? xinfo->strides : NULL;
    view->suboffsets = NULL;
    view->internal = xinfo;
    return 0;

  } catch (const std::exception& e) {
    view->obj = nullptr;
    delete xinfo;
    PyErr_SetString(PyExc_RuntimeError, e.what());
    return -1;
  }
}


static int dt_getbuffer_1_col(DataTable_PyObject *self, Py_buffer *view,
                              int flags)
{
  XInfo* xinfo = nullptr;
  MemoryBuffer* mbuf = nullptr;
  try {
    Column* col = self->ref->columns[0];
    mbuf = col->mbuf_shallowcopy();
    const char* fmt = format_from_stype(col->stype());

    xinfo = new XInfo();
    xinfo->mbuf = mbuf;
    xinfo->shape[0] = static_cast<Py_ssize_t>(col->nrows);
    xinfo->shape[1] = 1;
    xinfo->strides[0] = static_cast<Py_ssize_t>(col->elemsize());
    xinfo->strides[1] = static_cast<Py_ssize_t>(mbuf->size());

    view->buf = mbuf->get();
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

  } catch (const std::exception& e) {
    view->obj = nullptr;
    if (mbuf) mbuf->release();
    delete xinfo;
    PyErr_SetString(PyExc_RuntimeError, e.what());
    return -1;
  }
}


static int dt_getbuffer(DataTable_PyObject* self, Py_buffer* view, int flags)
{
  XInfo* xinfo = nullptr;
  MemoryBuffer* mbuf = nullptr;
  try {
    DataTable* dt = self->ref;
    size_t ncols = static_cast<size_t>(dt->ncols);
    size_t nrows = static_cast<size_t>(dt->nrows);

    if (ncols == 0) {
      return dt_getbuffer_no_cols(self, view, flags);
    }

    // Check whether we have a single-column DataTable that doesn't need to be
    // copied -- in which case it should be possible to return the buffer
    // by-reference instead of copying the data into an intermediate buffer.
    if (ncols == 1 && !dt->rowindex && !REQ_WRITABLE(flags) &&
        dt->columns[0]->is_fixedwidth()) {
      return dt_getbuffer_1_col(self, view, flags);
    }

    // Multiple columns datatable => copy all data into a new buffer before
    // passing it to the requester. This is of course very unfortunate, but
    // Numpy (the primary consumer of the buffer protocol) is unable to handle
    // "INDIRECT" buffer.

    // First, find the common stype for all columns in the DataTable.
    SType stype = self->use_stype_for_buffers;
    if (stype == ST_VOID) {
      // Auto-detect common stype
      uint64_t stypes_mask = 0;
      for (size_t i = 0; i < ncols; ++i) {
        SType next_stype = dt->columns[i]->stype();
        if (stypes_mask & (1 << next_stype)) continue;
        stypes_mask |= 1 << next_stype;
        stype = common_stype_for_buffer(stype, next_stype);
      }
    }

    // Allocate the final buffer
    assert(!stype_info[stype].varwidth);
    size_t elemsize = stype_info[stype].elemsize;
    size_t colsize = nrows * elemsize;
    mbuf = new MemoryMemBuf(ncols * colsize);
    const char* fmt = format_from_stype(stype);

    // Construct the data buffer
    for (size_t i = 0; i < ncols; ++i) {
      // either a shallow copy, or "materialized" column
      Column* col = dt->columns[i]->shallowcopy();
      col->reify();
      if (col->stype() == stype) {
        assert(col->alloc_size() == colsize);
        memcpy(mbuf->at(i*colsize), col->data(), colsize);
      } else {
        // xmb becomes a "view" on a portion of the buffer `mbuf`. An
        // ExternelMemBuf object is documented to be readonly; however in
        // practice it can still be written to, just not resized (this is
        // hacky, maybe fix in the future).
        MemoryBuffer* xmb = new ExternalMemBuf(mbuf->at(i*colsize), colsize);
        // Now we create a `newcol` by casting `col` into `stype`, using
        // the buffer `xmb`. Since `xmb` already has the correct size, this
        // is possible. The effect of this call is that `newcol` will be
        // created having the converted data; but the side-effect of this is
        // that `mbuf` will have the same data, and in the right place.
        Column* newcol = col->cast(stype, xmb);
        assert(newcol->alloc_size() == colsize);
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
    xinfo->mbuf = mbuf;
    xinfo->shape[0] = static_cast<Py_ssize_t>(nrows);
    xinfo->shape[1] = static_cast<Py_ssize_t>(ncols);
    xinfo->strides[0] = static_cast<Py_ssize_t>(elemsize);
    xinfo->strides[1] = static_cast<Py_ssize_t>(colsize);

    // Fill in the `view` struct
    view->buf = mbuf->get();
    view->obj = incref(reinterpret_cast<PyObject*>(self));
    view->len = static_cast<Py_ssize_t>(mbuf->size());
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
    PyErr_SetString(PyExc_RuntimeError, e.what());
    view->obj = nullptr;
    if (mbuf) mbuf->release();
    delete xinfo;
    return -1;
  }
}


/*
    There is a bug in Numpy where they request for an "indirect" buffer but then
    do not know how to handle it... In order to circumvent that we will always
    create "strides" buffer.
    https://github.com/numpy/numpy/issues/9456

    if (REQ_INDIRECT(flags)) {
        size_t elemsize = stype_info[stype].elemsize;
        Py_ssize_t *info = NULL;
        dtmalloc_g(info, Py_ssize_t, 6);
        void **buf = NULL;
        dtmalloc_g(buf, void*, ncols);
        for (int i = 0; i < ncols; i++) {
            dt->columns[i]->incref();
            buf[i] = dt->columns[i]->data;
        }

        view->buf = (void*) buf;
        view->obj = (PyObject*) self;
        view->len = ncols * dt->nrows * elemsize;
        view->readonly = 1;
        view->itemsize = elemsize;
        view->format = REQ_FORMAT(flags)? format_from_stype(stype) : NULL;
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

static void dt_releasebuffer(DataTable_PyObject*, Py_buffer *view)
{
  XInfo* xinfo = static_cast<XInfo*>(view->internal);
  if (xinfo->mbuf) xinfo->mbuf->release();
  delete xinfo;
}


PyBufferProcs datatable_as_buffer = {
  (getbufferproc) dt_getbuffer,
  (releasebufferproc) dt_releasebuffer,
};



//==============================================================================
// Buffers protocol for Python-side DataTable object
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

static int pydatatable_getbuffer(PyObject* self, Py_buffer* view, int flags)
{
  PyObject* pydt = PyObject_GetAttrString(self, "internal");
  if (pydt == nullptr) {
    PyErr_Format(PyExc_AttributeError,
                 "Cannot retrieve attribute internal");
    return -1;
  }
  return dt_getbuffer(reinterpret_cast<DataTable_PyObject*>(pydt), view, flags);
}

static void pydatatable_releasebuffer(PyObject*, Py_buffer* view)
{
  dt_releasebuffer(nullptr, view);
}

static PyBufferProcs pydatatable_as_buffer = {
  (getbufferproc) pydatatable_getbuffer,
  (releasebufferproc) pydatatable_releasebuffer,
};

PyObject* pyinstall_buffer_hooks(PyObject*, PyObject* args)
{
  PyObject* obj = nullptr;
  if (!PyArg_ParseTuple(args, "O", &obj)) return nullptr;
  obj->ob_type->tp_as_buffer = &pydatatable_as_buffer;
  return none();
}



//==============================================================================
// Buffers utility functions
//==============================================================================

static SType stype_from_format(const char* format, int64_t itemsize)
{
  SType stype = ST_VOID;
  char c = format[0];
  if (c == '@' || c == '=') c = format[1];

  if (c == 'b' || c == 'h' || c == 'i' || c == 'l' || c == 'q' || c == 'n') {
    // These are all various integer types
    stype = itemsize == 1 ? ST_INTEGER_I1 :
            itemsize == 2 ? ST_INTEGER_I2 :
            itemsize == 4 ? ST_INTEGER_I4 :
            itemsize == 8 ? ST_INTEGER_I8 : ST_VOID;
  }
  else if (c == 'd' || c == 'f') {
    stype = itemsize == 4 ? ST_REAL_F4 :
            itemsize == 8 ? ST_REAL_F8 : ST_VOID;
  }
  else if (c == '?') {
    stype = itemsize == 1 ? ST_BOOLEAN_I1 : ST_VOID;
  }
  else if (c == 'O') {
    stype = ST_OBJECT_PYPTR;
  }
  if (stype == ST_VOID) {
    PyErr_Format(PyExc_ValueError,
                 "Unknown format '%s' with itemsize %zd", format, itemsize);
  }
  return stype;
}

static const char* format_from_stype(SType stype)
{
  return stype == ST_BOOLEAN_I1? "?" :
         stype == ST_INTEGER_I1? "b" :
         stype == ST_INTEGER_I2? "h" :
         stype == ST_INTEGER_I4? "i" :
         stype == ST_INTEGER_I8? "q" :
         stype == ST_REAL_F4? "f" :
         stype == ST_REAL_F8? "d" :
         stype == ST_OBJECT_PYPTR? "O" : "x";
}
