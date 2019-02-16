//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#define dt_PY_COLUMN_cc
#include "py_column.h"
#include "py_rowindex.h"
#include "py_types.h"
#include "writebuf.h"
#include "python/list.h"

namespace pycolumn
{

pycolumn::obj* from_column(Column* col, pydatatable::obj* pydt, int64_t idx)
{
  PyObject* coltype = reinterpret_cast<PyObject*>(&pycolumn::type);
  PyObject* pyobj = PyObject_CallObject(coltype, nullptr);
  auto pycol = reinterpret_cast<pycolumn::obj*>(pyobj);
  if (!pycol || !col) throw PyError();
  pycol->ref = pydt? col->shallowcopy() : col;
  pycol->pydt = pydt;
  pycol->colidx = idx;
  Py_XINCREF(pydt);
  return pycol;
}


int unwrap(PyObject* object, Column** address) {
  if (!object) return 0;
  if (!PyObject_TypeCheck(object, &pycolumn::type)) {
    throw TypeError() << "Expected object of type Column";
  }
  *address = reinterpret_cast<pycolumn::obj*>(object)->ref;
  return 1;
}



//==============================================================================
// Column getters/setters
//==============================================================================

PyObject* get_mtype(pycolumn::obj* self) {
  return self->ref->mbuf_repr();
}


PyObject* get_stype(pycolumn::obj* self) {
  SType stype = self->ref->stype();
  return info(stype).py_stype().release();
}


PyObject* get_ltype(pycolumn::obj* self) {
  SType stype = self->ref->stype();
  return info(stype).py_ltype().release();
}


PyObject* get_data_size(pycolumn::obj* self) {
  Column* col = self->ref;
  return PyLong_FromSize_t(col->alloc_size());
}


PyObject* get_data_pointer(pycolumn::obj* self) {
  Column* col = self->ref;
  return PyLong_FromSize_t(reinterpret_cast<size_t>(col->data()));
}


PyObject* get_rowindex(pycolumn::obj* self) {
  Column* col = self->ref;
  const RowIndex& ri = col->rowindex();
  return (ri? py::orowindex(ri) : py::None()).release();
}



PyObject* get_refcount(pycolumn::obj*) {
  // "-1" because self->ref is a shallow copy of the "original" column, and
  // therefore it holds an extra reference to the data buffer.
  return PyLong_FromLong(/*self->ref->mbuf_refcount()*/ - 1);
}


PyObject* get_nrows(pycolumn::obj* self) {
  Column*& col = self->ref;
  return PyLong_FromSize_t(col->nrows);
}



//==============================================================================
// Column methods
//==============================================================================

PyObject* to_list(pycolumn::obj* self, PyObject*) {
  Column* col = self->ref;

  int itype = static_cast<int>(col->stype());
  auto formatter = py_stype_formatters[itype];
  py::olist out(col->nrows);

  col->rowindex().iterate(0, col->nrows, 1,
    [&](size_t i, size_t j) {
      out.set(i, j == RowIndex::NA
                        ? py::None()
                        : py::oobj::from_new_reference(formatter(col, j)));
    });

  return std::move(out).release();
}


static void dealloc(pycolumn::obj* self)
{
  delete self->ref;
  Py_XDECREF(self->pydt);
  self->ref = nullptr;
  self->pydt = nullptr;
  Py_TYPE(self)->tp_free(self);
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
  GETTER(rowindex),
  GETTER(refcount),
  GETTER(nrows),
  {nullptr, nullptr, nullptr, nullptr, nullptr}
};

static PyMethodDef column_methods[] = {
  METHOD0(to_list),
  {nullptr, nullptr, 0, nullptr}
};

PyTypeObject type = {
  PyVarObject_HEAD_INIT(nullptr, 0)
  cls_name,                           /* tp_name */
  sizeof(pycolumn::obj),              /* tp_basicsize */
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
  &pycolumn::as_buffer,               /* tp_as_buffer; see py_buffers.c */
  Py_TPFLAGS_DEFAULT,                 /* tp_flags */
  cls_doc,                            /* tp_doc */
  nullptr,                            /* tp_traverse */
  nullptr,                            /* tp_clear */
  nullptr,                            /* tp_richcompare */
  0      ,                            /* tp_weaklistoffset */
  nullptr,                            /* tp_iter */
  nullptr,                            /* tp_iternext */
  column_methods,                     /* tp_methods */
  nullptr,                            /* tp_members */
  column_getseters,                   /* tp_getset */
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


int static_init(PyObject* module) {
  // Register pycolumn::type on the module
  pycolumn::type.tp_new = PyType_GenericNew;
  if (PyType_Ready(&pycolumn::type) < 0) return 0;
  PyObject* typeobj = reinterpret_cast<PyObject*>(&type);
  Py_INCREF(typeobj);
  PyModule_AddObject(module, "Column", typeobj);

  return 1;
}


};  // namespace pycolumn
