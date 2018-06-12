//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
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
 *     Column** columns;
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

PyObject* columns_from_slice(PyObject*, PyObject *args) {
  PyObject *arg1, *arg2;
  int64_t start, count, step;
  if (!PyArg_ParseTuple(args, "OOLLL:columns_from_slice",
                        &arg1, &arg2, &start, &count, &step))
    return nullptr;
  DataTable* dt = PyObj(arg1).as_datatable();
  RowIndex rowindex = PyObj(arg2).as_rowindex();

  Column** columns = columns_from_slice(dt, rowindex, start, count, step);
  PyObject* res = wrap(columns, count);
  return res;
}


PyObject* columns_from_array(PyObject*, PyObject *args)
{
  PyObject *arg1, *arg2;
  PyObject* elems;
  if (!PyArg_ParseTuple(args, "OOO!:columns_from_array",
                        &arg1, &arg2, &PyList_Type, &elems))
    return nullptr;
  DataTable* dt = PyObj(arg1).as_datatable();
  RowIndex rowindex = PyObj(arg2).as_rowindex();

  int64_t ncols = PyList_Size(elems);
  int64_t* indices = dt::amalloc<int64_t>(ncols);
  for (int64_t i = 0; i < ncols; i++) {
    PyObject* elem = PyList_GET_ITEM(elems, i);
    indices[i] = (int64_t) PyLong_AsSize_t(elem);
  }

  Column** columns = columns_from_array(dt, rowindex, indices, ncols);
  PyObject* res = wrap(columns, ncols);
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
    return nullptr;

  columnset_mapfn* fnptr = (columnset_mapfn*) rawptr;
  int64_t ncols = PyList_Size(pyspec);
  int64_t* spec = dt::amalloc<int64_t>(ncols);
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
  Column** columns = dt::amalloc<Column*>(ncols + 1);
  for (int64_t i = 0; i < ncols; ++i) {
    PyObject* elem = PyList_GET_ITEM(col_list, i);
    int ret = pycolumn::unwrap(elem, columns + i);
    if (!ret) {
      for (int64_t j = 0; j < i; ++j) delete columns[j];
      dt::free(columns);
      return nullptr;
    }
    reinterpret_cast<pycolumn::obj*>(elem)->ref = nullptr;
    columns[i]->reify();
  }
  columns[ncols] = nullptr;

  return wrap(columns, ncols);
}



//==============================================================================
// Methods
//==============================================================================

PyObject* to_datatable(obj* self, PyObject*) {
  Column** columns = self->columns;
  self->columns = nullptr;
  return pydatatable::wrap(new DataTable(columns));
}


static void dealloc(obj* self)
{
  Column** ptr = self->columns;
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

static PyMethodDef methods[] = {
  METHOD0(to_datatable),
  {nullptr, nullptr, 0, nullptr}           /* sentinel */
};

PyTypeObject type = {
  PyVarObject_HEAD_INIT(NULL, 0)
  cls_name,                           /* tp_name */
  sizeof(pycolumnset::obj),           /* tp_basicsize */
  0,                                  /* tp_itemsize */
  DESTRUCTOR,                         /* tp_dealloc */
  nullptr,                            /* tp_print */
  nullptr,                            /* tp_getattr */
  nullptr,                            /* tp_setattr */
  nullptr,                            /* tp_compare */
  REPR,                               /* tp_repr */
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
  methods,                            /* tp_methods */
  nullptr,                            /* tp_members */
  nullptr,                            /* tp_getset */
  nullptr,                            /* tp_base */
  nullptr,                            /* tp_dict */
  nullptr,                            /* tp_descr_get */
  nullptr,                            /* tp_descr_set */
  0,                                  /* tp_dictoffset */
  nullptr,                            /* tp_init */
  nullptr,                            /* tp_alloc */
  nullptr,                            /* tp_new */
  nullptr,                            /* tp_free */
  nullptr,                            /* tp_is_gc */
  nullptr,                            /* tp_bases */
  nullptr,                            /* tp_mro */
  nullptr,                            /* tp_cache */
  nullptr,                            /* tp_subclasses */
  nullptr,                            /* tp_weaklist */
  nullptr,                            /* tp_del */
  0,                                  /* tp_version_tag */
  nullptr,                            /* tp_finalize */
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
#undef BASECLS
#undef CLSNAME
