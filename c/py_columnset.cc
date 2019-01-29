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
#include "frame/py_frame.h"
#include "py_column.h"
#include "py_datatable.h"
#include "py_rowindex.h"
#include "py_types.h"
#include "py_utils.h"
#include "utils.h"


namespace pycolumnset
{

static PyObject* wrap(Column** columns, size_t ncols)
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
  DataTable* dt = py::robj(arg1).to_frame();
  RowIndex rowindex = py::robj(arg2).to_rowindex();

  Column** columns = columns_from_slice(dt, rowindex, start, count, step);
  PyObject* res = wrap(columns, static_cast<size_t>(count));
  return res;
}


PyObject* columns_from_mixed(PyObject*, PyObject *args)
{
  PyObject* arg1, *arg2;
  size_t nrows;
  long long rawptr;
  if (!PyArg_ParseTuple(args, "OOlL:columns_from_mixed",
                        &arg1, &arg2, &nrows, &rawptr))
    return nullptr;
  py::olist pyspec = py::robj(arg1).to_pylist();
  DataTable* dt = py::robj(arg2).to_frame();

  columnset_mapfn* fnptr = reinterpret_cast<columnset_mapfn*>(rawptr);
  size_t ncols = pyspec.size();
  int64_t* spec = dt::amalloc<int64_t>(ncols);
  for (size_t i = 0; i < ncols; ++i) {
    auto elem = pyspec[i];
    if (elem.is_int()) {
      spec[i] = elem.to_int64_strict();
      xassert(spec[i] >= 0);
    } else {
      spec[i] = -elem.get_attr("itype").to_int64_strict();
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

  size_t ncols = static_cast<size_t>(PyList_Size(col_list));
  Column** columns = dt::amalloc<Column*>(ncols + 1);
  for (size_t i = 0; i < ncols; ++i) {
    PyObject* elem = PyList_GET_ITEM(col_list, i);
    int ret = pycolumn::unwrap(elem, columns + i);
    if (!ret) {
      for (size_t j = 0; j < i; ++j) delete columns[j];
      dt::free(columns);
      return nullptr;
    }
    reinterpret_cast<pycolumn::obj*>(elem)->ref = nullptr;
  }
  columns[ncols] = nullptr;

  return wrap(columns, ncols);
}



//==============================================================================
// Methods
//==============================================================================

PyObject* to_frame(obj* self, PyObject* args) {
  PyObject* arg1;
  if (!PyArg_ParseTuple(args, "O:to_frame", &arg1)) return nullptr;
  py::olist names = py::robj(arg1).to_pylist();

  std::vector<Column*> columns;
  for (Column** pcol = self->columns; *pcol; pcol++) {
    columns.push_back(*pcol);
  }
  self->columns = nullptr;
  DataTable* dt = new DataTable(std::move(columns), names);
  return py::Frame::from_datatable(dt);
}


PyObject* append_columns(obj* self, PyObject* args) {
  PyObject* arg1;
  if (!PyArg_ParseTuple(args, "O:append_columns", &arg1)) return nullptr;
  if (!PyObject_TypeCheck(arg1, &type)) {
    throw TypeError() << "Expected argument of type Columnset";
  }
  obj* other = static_cast<obj*>(arg1);

  size_t newncols = self->ncols + other->ncols;
  Column** columns = self->columns;
  columns = dt::arealloc<Column*>(columns, newncols + 1);
  std::memcpy(columns + self->ncols, other->columns,
              other->ncols * sizeof(Column*));
  columns[newncols] = nullptr;
  dt::free(other->columns);
  other->columns = nullptr;
  other->ncols = 0;
  self->columns = columns;
  self->ncols = newncols;
  return none();
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
  Py_TYPE(self)->tp_free(self);
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
  METHODv(to_frame),
  METHODv(append_columns),
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
  PyModule_AddObject(module, "ColumnSet", typeobj);
  return 1;
}


};  // namespace columnset
#undef BASECLS
#undef CLSNAME
