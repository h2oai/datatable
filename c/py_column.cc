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
#define dt_PY_COLUMN_cc
#include "py_column.h"
#include "sort.h"
#include "py_types.h"
#include "writebuf.h"
#include "utils/pyobj.h"

namespace pycolumn
{
PyObject* fn_hexview = NULL;  // see datatablemodule.c/pyregister_function


pycolumn::obj* from_column(Column* col, pydatatable::obj* pydt, int64_t idx)
{
  PyObject* coltype = reinterpret_cast<PyObject*>(&pycolumn::type);
  PyObject* pyobj = PyObject_CallObject(coltype, NULL);
  auto pycol = reinterpret_cast<pycolumn::obj*>(pyobj);
  if (!pycol || !col) throw PyError();
  pycol->ref = col->shallowcopy();
  pycol->pydt = pydt;
  pycol->colidx = idx;
  Py_XINCREF(pydt);
  return pycol;
}


int unwrap(PyObject* object, Column** address) {
  if (!object) return 0;
  if (!PyObject_TypeCheck(object, &pycolumn::type)) {
    PyErr_SetString(PyExc_TypeError, "Expected object of type Column");
    return 0;
  }
  *address = ((pycolumn::obj*)object)->ref;
  return 1;
}


PyObject* column_from_list(PyObject*, PyObject* args) {
  PyObject* list;
  if (!PyArg_ParseTuple(args, "O!", &PyList_Type, &list)) return nullptr;
  Column* col = Column::from_pylist(list);
  return from_column(col, nullptr, 0);
}



//==============================================================================
// Column getters/setters
//==============================================================================

PyObject* get_mtype(pycolumn::obj* self) {
  return self->ref->mbuf_repr();
}


PyObject* get_stype(pycolumn::obj* self) {
  SType stype = self->ref->stype();
  return incref(py_stype_names[stype]);
}


PyObject* get_ltype(pycolumn::obj* self) {
  SType stype = self->ref->stype();
  return incref(py_ltype_names[stype_info[stype].ltype]);
}


PyObject* get_data_size(pycolumn::obj* self) {
  Column* col = self->ref;
  return PyLong_FromSize_t(col->alloc_size());
}


PyObject* get_data_pointer(pycolumn::obj* self) {
  Column* col = self->ref;
  return PyLong_FromSize_t(reinterpret_cast<size_t>(col->data()));
}


PyObject* get_meta(pycolumn::obj*) {
  // Does nothing after the removal of metas in StringColumns.
  // Might be used when fixed-point decimals are implemented
  return none();
}


PyObject* get_refcount(pycolumn::obj* self) {
  // "-1" because self->ref is a shallow copy of the "original" column, and
  // therefore it holds an extra reference to the data buffer.
  return PyLong_FromLong(self->ref->mbuf_refcount() - 1);
}


PyObject* get_nrows(pycolumn::obj* self) {
  Column*& col = self->ref;
  return PyLong_FromLong(col->nrows);
}



//==============================================================================
// Column methods
//==============================================================================

PyObject* save_to_disk(pycolumn::obj* self, PyObject* args) {
  PyObject* arg1 = NULL;
  PyObject* arg2 = NULL;
  if (!PyArg_ParseTuple(args, "OO:save_to_disk", &arg1, &arg2))
    return NULL;
  PyObj pyfilename(arg1);
  PyObj pystrategy(arg2);

  Column* col = self->ref;
  const char* filename = pyfilename.as_cstring();
  std::string strategy = pystrategy.as_string();
  auto sstrategy = (strategy == "mmap")  ? WritableBuffer::Strategy::Mmap :
                   (strategy == "write") ? WritableBuffer::Strategy::Write :
                                           WritableBuffer::Strategy::Auto;

  col->save_to_disk(filename, sstrategy);
  Py_RETURN_NONE;
}


PyObject* hexview(pycolumn::obj* self, PyObject*)
{
  if (!fn_hexview) {
    throw RuntimeError() << "Function column_hexview() was not linked";
  }
  PyObject* v = Py_BuildValue("(OOi)", self, self->pydt, self->colidx);
  PyObject* ret = PyObject_CallObject(fn_hexview, v);
  Py_XDECREF(v);
  return ret;
}


static void dealloc(pycolumn::obj* self)
{
  delete self->ref;
  Py_XDECREF(self->pydt);
  self->ref = NULL;
  self->pydt = NULL;
  Py_TYPE(self)->tp_free((PyObject*)self);
}



//==============================================================================
// Column type definition
//==============================================================================

static PyGetSetDef column_getseters[] = {
  GETTER(mtype),
  GETTER(stype),
  GETTER(ltype),
  GETTER(data_size),
  GETTER(data_pointer),
  GETTER(meta),
  GETTER(refcount),
  GETTER(nrows),
  {NULL, NULL, NULL, NULL, NULL}
};

static PyMethodDef column_methods[] = {
  METHODv(save_to_disk),
  METHOD0(hexview),
  {NULL, NULL, 0, NULL}
};

PyTypeObject type = {
  PyVarObject_HEAD_INIT(NULL, 0)
  cls_name,                           /* tp_name */
  sizeof(pycolumn::obj),              /* tp_basicsize */
  0,                                  /* tp_itemsize */
  (destructor) dealloc,               /* tp_dealloc */
  0,                                  /* tp_print */
  0,                                  /* tp_getattr */
  0,                                  /* tp_setattr */
  0,                                  /* tp_compare */
  0,                                  /* tp_repr */
  0,                                  /* tp_as_number */
  0,                                  /* tp_as_sequence */
  0,                                  /* tp_as_mapping */
  0,                                  /* tp_hash  */
  0,                                  /* tp_call */
  0,                                  /* tp_str */
  0,                                  /* tp_getattro */
  0,                                  /* tp_setattro */
  &pycolumn::as_buffer,               /* tp_as_buffer; see py_buffers.c */
  Py_TPFLAGS_DEFAULT,                 /* tp_flags */
  cls_doc,                            /* tp_doc */
  0,                                  /* tp_traverse */
  0,                                  /* tp_clear */
  0,                                  /* tp_richcompare */
  0,                                  /* tp_weaklistoffset */
  0,                                  /* tp_iter */
  0,                                  /* tp_iternext */
  column_methods,                     /* tp_methods */
  0,                                  /* tp_members */
  column_getseters,                   /* tp_getset */
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


int static_init(PyObject* module) {
  init_sort_functions();

  // Register pycolumn::type on the module
  pycolumn::type.tp_new = PyType_GenericNew;
  if (PyType_Ready(&pycolumn::type) < 0) return 0;
  PyObject* typeobj = reinterpret_cast<PyObject*>(&type);
  Py_INCREF(typeobj);
  PyModule_AddObject(module, "Column", typeobj);

  return 1;
}


};  // namespace pycolumn
