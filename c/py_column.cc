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
#define PY_COLUMN_cc 1
#include "py_column.h"
#include "sort.h"
#include "py_types.h"



Column_PyObject* pycolumn_from_column(
    Column *col, DataTable_PyObject *pydt, int64_t colidx)
{
  PyObject* coltype = reinterpret_cast<PyObject*>(&Column_PyType);
  PyObject* pyobj = PyObject_CallObject(coltype, NULL);
  Column_PyObject* pycol = reinterpret_cast<Column_PyObject*>(pyobj);
  if (!pycol) throw PyError();
  pycol->ref = col->shallowcopy();
  pycol->pydt = pydt;
  pycol->colidx = colidx;
  Py_XINCREF(pydt);
  return pycol;
}



//==============================================================================
// Column getters/setters
//==============================================================================

PyObject* get_mtype(Column_PyObject* self) {
  return self->ref->mbuf_repr();
}


PyObject* get_stype(Column_PyObject* self) {
  SType stype = self->ref->stype();
  return incref(py_stype_names[stype]);
}


PyObject* get_ltype(Column_PyObject* self) {
  SType stype = self->ref->stype();
  return incref(py_ltype_names[stype_info[stype].ltype]);
}


PyObject* get_data_size(Column_PyObject *self) {
  Column* col = self->ref;
  return PyLong_FromSize_t(col->alloc_size());
}


PyObject* get_data_pointer(Column_PyObject *self) {
  Column* col = self->ref;
  return PyLong_FromSize_t(reinterpret_cast<size_t>(col->data()));
}


PyObject* get_meta(Column_PyObject* self) {
  Column* col = self->ref;
  void* meta = col->meta;
  switch (col->stype()) {
    case ST_STRING_I4_VCHAR:
    case ST_STRING_I8_VCHAR:
      return PyUnicode_FromFormat("offoff=%lld",
                                  ((VarcharMeta*)meta)->offoff);
    default:
      return none();
  }
}


PyObject* get_refcount(Column_PyObject* self) {
  // "-1" because self->ref is a shallow copy of the "original" column, and
  // therefore it holds an extra reference to the data buffer.
  return PyLong_FromLong(self->ref->mbuf_refcount() - 1);
}



//==============================================================================
// Column methods
//==============================================================================

PyObject* save_to_disk(Column_PyObject* self, PyObject* args) {
  const char* filename = NULL;
  if (!PyArg_ParseTuple(args, "s:save_to_disk", &filename)) return NULL;
  Column* col = self->ref;
  col->save_to_disk(filename);
  Py_RETURN_NONE;
}


PyObject* hexview(Column_PyObject* self, PyObject*)
{
  if (!pyfn_column_hexview) {
    throw RuntimeError() << "Function column_hexview() was not linked";
  }
  PyObject* v = Py_BuildValue("(OOi)", self, self->pydt, self->colidx);
  PyObject* ret = PyObject_CallObject(pyfn_column_hexview, v);
  Py_XDECREF(v);
  return ret;
}


static void pycolumn_dealloc(Column_PyObject *self)
{
  // FIXME!
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
  {NULL, NULL, NULL, NULL, NULL}
};

static PyMethodDef column_methods[] = {
  METHODv(save_to_disk),
  METHOD0(hexview),
  {NULL, NULL, 0, NULL}
};

PyTypeObject Column_PyType = {
  PyVarObject_HEAD_INIT(NULL, 0)
  "_datatable.Column",                /* tp_name */
  sizeof(Column_PyObject),            /* tp_basicsize */
  0,                                  /* tp_itemsize */
  (destructor)pycolumn_dealloc,       /* tp_dealloc */
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
  &column_as_buffer,                  /* tp_as_buffer; see py_buffers.c */
  Py_TPFLAGS_DEFAULT,                 /* tp_flags */
  "Column object",                    /* tp_doc */
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


int init_py_column(PyObject *module) {
  init_sort_functions();

  // Register Column_PyType on the module
  Column_PyType.tp_new = PyType_GenericNew;
  if (PyType_Ready(&Column_PyType) < 0) return 0;
  Py_INCREF(&Column_PyType);
  PyModule_AddObject(module, "Column",
                     reinterpret_cast<PyObject*>(&Column_PyType));

  return 1;
}
