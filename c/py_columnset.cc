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
#define dt_PY_COLUMNSET_cc
#include "py_columnset.h"
#include "columnset.h"
#include "py_column.h"
#include "py_datatable.h"
#include "py_rowindex.h"
#include "py_types.h"
#include "py_utils.h"
#include "utils.h"
#include "utils/pyobj.h"


namespace pycolumnset
{

static PyObject* wrap(Column** columns, int64_t ncols)
{
  if (!columns) return nullptr;
  PyObject* pytype = reinterpret_cast<PyObject*>(&type);
  PyObject* result = PyObject_CallObject(pytype, nullptr);
  if (!result) return nullptr;
  static_cast<obj*>(result)->columns = columns;
  static_cast<obj*>(result)->ncols = ncols;
  return result;
}


/**
 * Helper function to be used with `PyArg_ParseTuple()` in order to extract
 * a `Column**` pointer out of the arguments tuple. Usage:
 *
 *     Column **columns;
 *     if (!PyArg_ParseTuple(args, "O&", &columnset_unwrap, &columns))
 *         return NULL;
 */
int unwrap(PyObject* object, void* address) {
  Column*** ans = static_cast<Column***>(address);
  if (!PyObject_TypeCheck(object, &type)) {
    PyErr_SetString(PyExc_TypeError,
                    "Expected argument of type ColumnSet");
    return 0;
  }
  *ans = static_cast<obj*>(object)->columns;
  return 1;
}


//==============================================================================

PyObject* columns_from_slice(PyObject*, PyObject *args)
{
  DataTable *dt;
  int64_t start, count, step;
  if (!PyArg_ParseTuple(args, "O&LLL:columns_from_slice",
                        &pydatatable::unwrap, &dt, &start, &count, &step))
    return NULL;

  PyObject* res = wrap(columns_from_slice(dt, start, count, step), count);
  return res;
}


PyObject* columns_from_array(PyObject*, PyObject *args)
{
  DataTable* dt;
  PyObject* elems;
  if (!PyArg_ParseTuple(args, "O&O!:columns_from_array",
                        &pydatatable::unwrap, &dt, &PyList_Type, &elems))
    return NULL;

  int64_t ncols = PyList_Size(elems);
  int64_t* indices = NULL;
  dtmalloc(indices, int64_t, ncols);
  for (int64_t i = 0; i < ncols; i++) {
    PyObject* elem = PyList_GET_ITEM(elems, i);
    indices[i] = (int64_t) PyLong_AsSize_t(elem);
  }

  PyObject* res = wrap(columns_from_array(dt, indices, ncols), ncols);
  return res;
}


PyObject* columns_from_mixed(PyObject*, PyObject *args)
{
  PyObject* pyspec;
  DataTable* dt;
  long int nrows;
  long long rawptr;
  if (!PyArg_ParseTuple(args, "O!O&lL:columns_from_mixed",
                        &PyList_Type, &pyspec, &pydatatable::unwrap, &dt,
                        &nrows, &rawptr))
    return NULL;

  columnset_mapfn* fnptr = (columnset_mapfn*) rawptr;
  int64_t ncols = PyList_Size(pyspec);
  int64_t* spec = NULL;

  dtmalloc(spec, int64_t, ncols);
  for (int64_t i = 0; i < ncols; i++) {
    PyObject* elem = PyList_GET_ITEM(pyspec, i);
    if (PyLong_CheckExact(elem)) {
      spec[i] = PyLong_AsInt64(elem);
    } else {
      spec[i] = -PyObj(elem, "itype").as_int64();
    }
  }
  return wrap(columns_from_mixed(spec, ncols, nrows, dt, fnptr), ncols);
}


PyObject* columns_from_columns(PyObject*, PyObject* args)
{
  PyObject* col_list;
  if (!PyArg_ParseTuple(args, "O!:columns_from_columns",
                        &PyList_Type, &col_list))
    return nullptr;

  int64_t ncols = PyList_Size(col_list);
  Column** columns;
  dtmalloc(columns, Column*, ncols + 1);
  for (int64_t i = 0; i < ncols; ++i) {
    PyObject* elem = PyList_GET_ITEM(col_list, i);
    int ret = pycolumn::unwrap(elem, columns + i);
    if (!ret) {
      for (int64_t j = 0; j < i; ++j) delete columns[j];
      dtfree(columns);
      return nullptr;
    }
    reinterpret_cast<pycolumn::obj*>(elem)->ref = nullptr;
  }
  columns[ncols] = nullptr;

  return wrap(columns, ncols);
}


static void dealloc(obj* self)
{
  Column **ptr = self->columns;
  if (ptr) {
    while (*ptr) {
      free(*ptr);
      ptr++;
    }
    free(self->columns);
  }
  Py_TYPE(self)->tp_free((PyObject*)self);
}


static PyObject* repr(obj* self)
{
  Column** ptr = self->columns;
  if (!ptr)
    return PyUnicode_FromString("_ColumnSet(NULL)");
  int ncols = 0;
  while (ptr[ncols]) ncols++;
  return PyUnicode_FromFormat("_ColumnSet(ncols=%d)", ncols);
}



//==============================================================================
// ColumnSet type definition
//==============================================================================

PyTypeObject type = {
  PyVarObject_HEAD_INIT(NULL, 0)
  cls_name,                           /* tp_name */
  sizeof(pycolumnset::obj),           /* tp_basicsize */
  0,                                  /* tp_itemsize */
  (destructor)dealloc,                /* tp_dealloc */
  0,                                  /* tp_print */
  0,                                  /* tp_getattr */
  0,                                  /* tp_setattr */
  0,                                  /* tp_compare */
  (reprfunc)repr,                     /* tp_repr */
  0,                                  /* tp_as_number */
  0,                                  /* tp_as_sequence */
  0,                                  /* tp_as_mapping */
  0,                                  /* tp_hash  */
  0,                                  /* tp_call */
  0,                                  /* tp_str */
  0,                                  /* tp_getattro */
  0,                                  /* tp_setattro */
  0,                                  /* tp_as_buffer */
  Py_TPFLAGS_DEFAULT,                 /* tp_flags */
  cls_doc,                            /* tp_doc */
  0,                                  /* tp_traverse */
  0,                                  /* tp_clear */
  0,                                  /* tp_richcompare */
  0,                                  /* tp_weaklistoffset */
  0,                                  /* tp_iter */
  0,                                  /* tp_iternext */
  0,                                  /* tp_methods */
  0,                                  /* tp_members */
  0,                                  /* tp_getset */
  0,                                  /* tp_base */
  0,                                  /* tp_dict */
  0,                                  /* tp_descr_get */
  0,                                  /* tp_descr_set */
  0,                                  /* tp_dictoffset */
  0,                                  /* tp_init */
  0,                                  /* tp_alloc */
  0,                                  /* tp_new */
  0,                                  /* tp_free */
  0,                                  /* tp_is_gc */
  0,                                  /* tp_bases */
  0,                                  /* tp_mro */
  0,                                  /* tp_cache */
  0,                                  /* tp_subclasses */
  0,                                  /* tp_weaklist */
  0,                                  /* tp_del */
  0,                                  /* tp_version_tag */
  0,                                  /* tp_finalize */
};


// Add PyColumnSet object to the Python module
int static_init(PyObject* module)
{
  // Register pycolumnset::type within the module
  type.tp_new = PyType_GenericNew;
  if (PyType_Ready(&type) < 0) return 0;
  PyObject* typeobj = reinterpret_cast<PyObject*>(&type);
  Py_INCREF(typeobj);
  PyModule_AddObject(module, "ColumnSet", (PyObject*) &type);
  return 1;
}


};  // namespace columnset
