//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#define dt_PY_DATAWINDOW_cc
#include "py_datawindow.h"
#include <Python.h>
#include <structmember.h>
#include "datatable.h"
#include "rowindex.h"
#include "py_datatable.h"
#include "py_types.h"
#include "py_utils.h"


namespace pydatawindow
{

// Forward declarations
static int _init_hexview(obj* self, DataTable* dt, int64_t column,
  int64_t row0, int64_t row1, int64_t col0, int64_t col1);

static PyObject* hexcodes[257];



/**
 * DataWindow object constructor. This constructor takes a datatable, and
 * coordinates of a data window, and extracts the data from the datatable
 * within the provided window as a pythonic list of lists.
 *
 * :param dt
 *     The datatable whose data window we want to extract.
 *
 * :param row0, row1
 *     Index of the first / last+1 row of the data window.
 *
 * :param col0, col1
 *     Index of the first / last+1 column of the data window.
 *
 * :param column (optional)
 *     If specified, then a "hex view" data window will be returned instead of
 *     the regular data window. The `column` parameter specifies the index of
 *     the column (within the datatable `dt`) whose binary data is accessed. The
 *     rows of the returned datawindow correspond to 16-byte chunks of the
 *     column's binary data. There will be 17 columns in the returned data
 *     window, first 16 are hex representations of each byte within the 16-byte
 *     chunk, and the last column is ASCII rendering of the same chunk.
 */
static int _init_(obj* self, PyObject* args, PyObject* kwds)
{
  pydatatable::obj *pydt;
  DataTable *dt;
  PyObject *stypes = nullptr, *ltypes = nullptr, *view = nullptr;
  int n_init_cols = 0;
  int64_t row0, row1, col0, col1;
  int64_t column = -1;

  // Parse arguments and check their validity
  static const char* kwlist[] =
    {"dt", "row0", "row1", "col0", "col1", "column", nullptr};
  int ret = PyArg_ParseTupleAndKeywords(
    args, kwds, "O!nnnn|n:DataWindow.__init__", const_cast<char **>(kwlist),
    &pydatatable::type, &pydt, &row0, &row1, &col0, &col1, &column
  );
  if (!ret) return -1;

  try {
  dt = pydt == nullptr? nullptr : pydt->ref;
  if (column >= 0) {
    return _init_hexview(self, dt, column, row0, row1, col0, col1);
  }

  if (col0 < 0 || col1 < col0 || col1 > dt->ncols ||
    row0 < 0 || row1 < row0 || row1 > dt->nrows) {
    throw ValueError() <<
      "Invalid data window bounds: Frame is [" << dt->nrows << " x " <<
      dt->ncols << "], whereas requested window is [" << row0 << ".." <<
      row1 << " x " << col0 << ".." << col1 << "]";
  }

  // Window dimensions
  int64_t ncols = col1 - col0;
  int64_t nrows = row1 - row0;

  RowIndex rindex(dt->rowindex);
  int no_rindex = rindex.isabsent();
  int rindex_is_arr32 = rindex.isarr32();
  int rindex_is_arr64 = rindex.isarr64();
  int rindex_is_slice = rindex.isslice();
  const int32_t* rindexarr32 = rindex_is_arr32? rindex.indices32() : nullptr;
  const int64_t* rindexarr64 = rindex_is_arr64? rindex.indices64() : nullptr;
  int64_t rindexstart = rindex_is_slice? rindex.slice_start() : 0;
  int64_t rindexstep = rindex_is_slice? rindex.slice_step() : 0;

  // Create and fill-in the `data` list
  view = PyList_New((Py_ssize_t) ncols);
  if (view == nullptr) goto fail;
  for (int64_t i = col0; i < col1; ++i) {
    Column *col = dt->columns[i];

    PyObject *py_coldata = PyList_New((Py_ssize_t) nrows);
    if (py_coldata == nullptr) goto fail;
    PyList_SET_ITEM(view, n_init_cols++, py_coldata);

    int n_init_rows = 0;
    for (int64_t j = row0; j < row1; ++j) {
      int64_t irow = no_rindex? j :
               rindex_is_arr32? rindexarr32[j] :
               rindex_is_arr64? (int64_t) rindexarr64[j] :
                      rindexstart + rindexstep * j;
      PyObject *value = py_stype_formatters[col->stype()](col, irow);
      if (value == nullptr) goto fail;
      PyList_SET_ITEM(py_coldata, n_init_rows++, value);
    }
  }

  // Create and fill-in the `stypes` list
  stypes = PyList_New((Py_ssize_t) ncols);
  ltypes = PyList_New((Py_ssize_t) ncols);
  if (stypes == nullptr || ltypes == nullptr) goto fail;
  for (int64_t i = col0; i < col1; i++) {
    Column *col = dt->columns[i];
    SType stype = col->stype();
    LType ltype = stype_info[stype].ltype;
    PyList_SET_ITEM(ltypes, i - col0, incref(py_ltype_names[ltype]));
    PyList_SET_ITEM(stypes, i - col0, incref(py_stype_names[stype]));
  }

  self->row0 = row0;
  self->row1 = row1;
  self->col0 = col0;
  self->col1 = col1;
  self->types = (PyListObject*) ltypes;
  self->stypes = (PyListObject*) stypes;
  self->data = (PyListObject*) view;
  return 0;
  } catch (const std::exception& e) {
  exception_to_python(e);
  // fall-through into 'fail'
  }

  fail:
  Py_XDECREF(stypes);
  Py_XDECREF(ltypes);
  Py_XDECREF(view);
  return -1;
}



static int _init_hexview(
  obj* self, DataTable* dt, int64_t colidx,
  int64_t row0, int64_t row1, int64_t col0, int64_t col1)
{
  PyObject *viewdata = nullptr;
  PyObject *ltypes = nullptr;
  PyObject *stypes = nullptr;

  try {
  if (colidx < 0 || colidx >= dt->ncols) {
    PyErr_Format(PyExc_ValueError, "Invalid column index %lld", colidx);
    return -1;
  }
  Column *column = dt->columns[colidx];

  int64_t maxrows = ((int64_t) column->alloc_size() + 15) >> 4;
  if (col0 < 0 || col1 < col0 || col1 > 17 ||
    row0 < 0 || row1 < row0 || row1 > maxrows) {
    PyErr_Format(PyExc_ValueError,
      "Invalid data window bounds: [%ld..%ld x %ld..%ld] "
      "for a column with allocation size %zd",
      row0, row1, col0, col1, column->alloc_size());
    return -1;
  }
  // Window dimensions
  int64_t ncols = col1 - col0;
  int64_t nrows = row1 - row0;

  const uint8_t* ptr0 = static_cast<const uint8_t*>(column->data());
  const uint8_t* coldata = ptr0 + 16 * row0;
  const uint8_t* coldata_end = ptr0 + column->alloc_size();
  viewdata = PyList_New(ncols);
  if (!viewdata) goto fail;
  for (int i = 0; i < ncols; i++) {
    PyObject *py_coldata = PyList_New(nrows);
    if (!py_coldata) goto fail;
    PyList_SET_ITEM(viewdata, i, py_coldata);

    if (i < 16) {
      for (int j = 0; j < nrows; j++) {
        const uint8_t* ch = coldata + i + (j * 16);
        PyList_SET_ITEM(py_coldata, j,
          incref(ch < coldata_end? hexcodes[*ch] : hexcodes[256]));
      }
    }
    if (i == 16) {
      for (int j = 0; j < nrows; j++) {
        char buf[16];
        for (int k = 0; k < 16; k++) {
          const uint8_t* ch = coldata + k + (j * 16);
          buf[k] = ch >= coldata_end? ' ' :
               (*ch < 0x20 ||( *ch >= 0x7F && *ch < 0xA0))? '.' :
               (char) *ch;
        }
        PyObject *str = PyUnicode_Decode(buf, 16, "Latin1", "strict");
        if (!str) goto fail;
        PyList_SET_ITEM(py_coldata, j, str);
      }
    }
  }

  // Create and fill-in the `stypes`/`ltypes` lists
  stypes = PyList_New(ncols);
  ltypes = PyList_New(ncols);
  if (!stypes || !ltypes) goto fail;
  for (int64_t i = 0; i < ncols; i++) {
    PyList_SET_ITEM(ltypes, i, incref(py_ltype_names[LT_STRING]));
    PyList_SET_ITEM(stypes, i, incref(py_stype_names[ST_STRING_FCHAR]));
  }

  self->row0 = row0;
  self->row1 = row1;
  self->col0 = col0;
  self->col1 = col1;
  self->data = (PyListObject*) viewdata;
  self->types = (PyListObject*) ltypes;
  self->stypes = (PyListObject*) stypes;
  return 0;
  } catch (const std::exception& e) {
  exception_to_python(e);
  // fall-through into 'fail'
  }

  fail:
  Py_XDECREF(viewdata);
  Py_XDECREF(stypes);
  Py_XDECREF(ltypes);
  return -1;
}



//==============================================================================
// Getters/setters
//==============================================================================

PyObject* get_row0(obj* self) {
  return PyLong_FromLongLong(self->row0);
}

PyObject* get_row1(obj* self) {
  return PyLong_FromLongLong(self->row1);
}

PyObject* get_col0(obj* self) {
  return PyLong_FromLongLong(self->col0);
}

PyObject* get_col1(obj* self) {
  return PyLong_FromLongLong(self->col1);
}

PyObject* get_types(obj* self) {
  Py_INCREF(self->types);
  return reinterpret_cast<PyObject*>(self->types);
}

PyObject* get_stypes(obj* self) {
  Py_INCREF(self->stypes);
  return reinterpret_cast<PyObject*>(self->stypes);
}

PyObject* get_data(obj* self) {
  Py_INCREF(self->data);
  return reinterpret_cast<PyObject*>(self->data);
}




//==============================================================================

/**
 * Destructor
 */
static void dealloc(obj* self)
{
  Py_XDECREF(self->data);
  Py_XDECREF(self->types);
  Py_XDECREF(self->stypes);
  Py_TYPE(self)->tp_free((PyObject*)self);
}



//------ Declare the DataWindow object -----------------------------------------

static PyGetSetDef getsetters[] = {
  GETTER(row0),
  GETTER(row1),
  GETTER(col0),
  GETTER(col1),
  GETTER(types),
  GETTER(stypes),
  GETTER(data),
  {nullptr, nullptr, nullptr, nullptr, nullptr}  /* sentinel */
};


PyTypeObject type = {
  PyVarObject_HEAD_INIT(nullptr, 0)
  cls_name,                           /* tp_name */
  sizeof(obj),                        /* tp_basicsize */
  0,                                  /* tp_itemsize */
  DESTRUCTOR,                         /* tp_dealloc */
  nullptr,                            /* tp_print */
  nullptr,                            /* tp_getattr */
  nullptr,                            /* tp_setattr */
  nullptr,                            /* tp_compare */
  nullptr,                            /* tp_repr */
  nullptr,                            /* tp_as_number */
  nullptr,                            /* tp_as_sequence */
  nullptr,                            /* tp_as_mapping */
  nullptr,                            /* tp_hash  */
  nullptr,                            /* tp_call */
  nullptr,                            /* tp_str */
  nullptr,                            /* tp_getattro */
  nullptr,                            /* tp_setattro */
  nullptr,                            /* tp_as_buffer */
  Py_TPFLAGS_DEFAULT,                 /* tp_flags */
  cls_doc,                            /* tp_doc */
  nullptr,                            /* tp_traverse */
  nullptr,                            /* tp_clear */
  nullptr,                            /* tp_richcompare */
  0,                                  /* tp_weaklistoffset */
  nullptr,                            /* tp_iter */
  nullptr,                            /* tp_iternext */
  nullptr,                            /* tp_methods */
  nullptr,                            /* tp_members */
  getsetters,                         /* tp_getset */
  nullptr,                            /* tp_base */
  nullptr,                            /* tp_dict */
  nullptr,                            /* tp_descr_get */
  nullptr,                            /* tp_descr_set */
  0,                                  /* tp_dictoffset */
  CONSTRUCTOR,                        /* tp_init */
  nullptr,
  nullptr,
  nullptr,
  nullptr,
  nullptr,
  nullptr,
  nullptr,
  nullptr,
  nullptr,
  nullptr,
  0,
  nullptr,
};


// Module initialization
int static_init(PyObject* module) {
  type.tp_new = PyType_GenericNew;
  if (PyType_Ready(&type) < 0) return 0;
  Py_INCREF(&type);
  PyModule_AddObject(module, "DataWindow", (PyObject*) &type);

  for (int i = 0; i < 256; i++) {
    int8_t d0 = i & 0x0F;
    int8_t d1 = (i >> 4) & 0x0F;
    char buf[2];
    buf[0] = d1 < 10? ('0' + d1) : ('A' + (d1 - 10));
    buf[1] = d0 < 10? ('0' + d0) : ('A' + (d0 - 10));
    hexcodes[i] = PyUnicode_FromStringAndSize(buf, 2);
  }
  hexcodes[256] = PyUnicode_FromStringAndSize("  ", 2);
  return 1;
}

};  // namespace pydatawindow
