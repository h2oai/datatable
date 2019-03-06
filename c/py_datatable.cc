//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#define dt_PY_DATATABLE_cc
#include "py_datatable.h"
#include <exception>
#include <iostream>
#include <vector>
#include "datatable.h"
#include "datatablemodule.h"
#include "frame/py_frame.h"
#include "py_column.h"
#include "py_rowindex.h"
#include "py_types.h"
#include "py_utils.h"
#include "python/int.h"
#include "python/list.h"
#include "python/string.h"


namespace pydatatable
{


/**
 * Create a new `pydatatable::obj` by wrapping the provided DataTable `dt`.
 * If `dt` is nullptr then this function also returns nullptr.
 */
PyObject* wrap(DataTable* dt)
{
  if (!dt) return nullptr;
  PyObject* pytype = reinterpret_cast<PyObject*>(&pydatatable::type);
  PyObject* pydt = PyObject_CallObject(pytype, nullptr);
  if (pydt) {
    auto pypydt = reinterpret_cast<pydatatable::obj*>(pydt);
    pypydt->ref = dt;
    pypydt->_frame = nullptr;
  }
  return pydt;
}


/**
 * Attempt to extract a DataTable from the given PyObject, iff it is a
 * pydatatable::obj. On success, the reference to the DataTable is stored in
 * the `address` and 1 is returned. On failure, returns 0.
 *
 * This function does not attempt to DECREF the source PyObject.
 */
int unwrap(PyObject* object, DataTable** address) {
  if (!object) return 0;
  if (!PyObject_TypeCheck(object, &pydatatable::type)) {
    PyErr_SetString(PyExc_TypeError, "Expected object of type DataTable");
    return 0;
  }
  *address = static_cast<pydatatable::obj*>(object)->ref;
  return 1;
}




//==============================================================================
// PyDatatable getters/setters
//==============================================================================

PyObject* get_isview(obj* self) {
  DataTable* dt = self->ref;
  for (size_t i = 0; i < dt->ncols; ++i) {
    if (dt->columns[i]->rowindex())
      return incref(Py_True);
  }
  return incref(Py_False);
}



PyObject* get_datatable_ptr(obj* self) {
  return PyLong_FromLongLong(reinterpret_cast<long long int>(self->ref));
}



//==============================================================================
// PyDatatable methods
//==============================================================================

void _clear_types(obj* self) {
  if (self->_frame) self->_frame->_clear_types();
}



PyObject* check(obj* self, PyObject*) {
  DataTable* dt = self->ref;
  dt->verify_integrity();

  if (self->_frame && self->_frame->stypes) {
    PyObject* stypes = self->_frame->stypes;
    if (!PyTuple_Check(stypes)) {
      throw AssertionError() << "Frame.stypes is not a tuple";
    }
    if (static_cast<size_t>(PyTuple_Size(stypes)) != dt->ncols) {
      throw AssertionError() << "len(Frame.stypes) is " << PyTuple_Size(stypes)
          << ", whereas .ncols = " << dt->ncols;
    }
    for (size_t i = 0; i < dt->ncols; ++i) {
      SType st = dt->columns[i]->stype();
      PyObject* elem = PyTuple_GET_ITEM(stypes, i);
      PyObject* eexp = info(st).py_stype().release();
      if (elem != eexp) {
        throw AssertionError() << "Element " << i << " of Frame.stypes is "
            << elem << ", but the column's type is " << eexp;
      }
    }
  }
  if (self->_frame && self->_frame->ltypes) {
    PyObject* ltypes = self->_frame->ltypes;
    if (!PyTuple_Check(ltypes)) {
      throw AssertionError() << "Frame.ltypes is not a tuple";
    }
    if (static_cast<size_t>(PyTuple_Size(ltypes)) != dt->ncols) {
      throw AssertionError() << "len(Frame.ltypes) is " << PyTuple_Size(ltypes)
          << ", whereas .ncols = " << dt->ncols;
    }
    for (size_t i = 0; i < dt->ncols; ++i) {
      SType st = dt->columns[i]->stype();
      PyObject* elem = PyTuple_GET_ITEM(ltypes, i);
      PyObject* eexp = info(st).py_ltype().release();
      if (elem != eexp) {
        throw AssertionError() << "Element " << i << " of Frame.ltypes is "
            << elem << ", for a column of type " << eexp;
      }
    }
  }

  Py_RETURN_NONE;
}


PyObject* column(obj* self, PyObject* args) {
  DataTable* dt = self->ref;
  int64_t colidx;
  int64_t ncols = static_cast<int64_t>(dt->ncols);
  if (!PyArg_ParseTuple(args, "l:column", &colidx))
    return nullptr;
  if (colidx < -ncols || colidx >= ncols) {
    PyErr_Format(PyExc_ValueError, "Invalid column index %lld", colidx);
    return nullptr;
  }
  if (colidx < 0) colidx += dt->ncols;
  pycolumn::obj* pycol =
      pycolumn::from_column(dt->columns[static_cast<size_t>(colidx)], self, colidx);
  return pycol;
}



PyObject* materialize(obj* self, PyObject*) {
  DataTable* dt = self->ref;
  dt->reify();
  Py_RETURN_NONE;
}



//------------------------------------------------------------------------------
// Misc
//------------------------------------------------------------------------------

static int __init__(PyObject* self, PyObject*, PyObject*) {
  TRACK(self, sizeof(pydatatable::obj), "pydatatable::obj");
  return 0;
}

static void dealloc(obj* self) {
  delete self->ref;
  Py_TYPE(self)->tp_free(self);
  UNTRACK(self);
}



//==============================================================================
// DataTable type definition
//==============================================================================

static PyMethodDef datatable_methods[] = {
  METHOD0(check),
  METHODv(column),
  METHOD0(materialize),
  {nullptr, nullptr, 0, nullptr}           /* sentinel */
};

static PyGetSetDef datatable_getseters[] = {
  GETTER(isview),
  GETTER(datatable_ptr),
  {nullptr, nullptr, nullptr, nullptr, nullptr}  /* sentinel */
};

PyTypeObject type = {
  PyVarObject_HEAD_INIT(nullptr, 0)
  cls_name,                           /* tp_name */
  sizeof(pydatatable::obj),           /* tp_basicsize */
  0,                                  /* tp_itemsize */
  DESTRUCTOR,                         /* tp_dealloc */
  nullptr,                            /* tp_print */
  nullptr,                            /* tp_getattr */
  nullptr,                            /* tp_setattr */
  nullptr,                            /* tp_as_sync */
  nullptr,                            /* tp_repr */
  nullptr,                            /* tp_as_number */
  nullptr,                            /* tp_as_sequence */
  nullptr,                            /* tp_as_mapping */
  nullptr,                            /* tp_hash  */
  nullptr,                            /* tp_call */
  nullptr,                            /* tp_str */
  nullptr,                            /* tp_getattro */
  nullptr,                            /* tp_setattro */
  &pydatatable::as_buffer,            /* tp_as_buffer;  see py_buffers.cc */
  Py_TPFLAGS_DEFAULT,                 /* tp_flags */
  cls_doc,                            /* tp_doc */
  nullptr,                            /* tp_traverse */
  nullptr,                            /* tp_clear */
  nullptr,                            /* tp_richcompare */
  0,                                  /* tp_weaklistoffset */
  nullptr,                            /* tp_iter */
  nullptr,                            /* tp_iternext */
  datatable_methods,                  /* tp_methods */
  nullptr,                            /* tp_members */
  datatable_getseters,                /* tp_getset */
  nullptr,                            /* tp_base */
  nullptr,                            /* tp_dict */
  nullptr,                            /* tp_descr_get */
  nullptr,                            /* tp_descr_set */
  0,                                  /* tp_dictoffset */
  __init__,                           /* tp_init */
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
  // Register type on the module
  pydatatable::type.tp_new = PyType_GenericNew;
  if (PyType_Ready(&pydatatable::type) < 0) return 0;
  PyObject* typeobj = reinterpret_cast<PyObject*>(&type);
  Py_INCREF(typeobj);
  PyModule_AddObject(module, "DataTable", typeobj);

  return 1;
}


};  // namespace pydatatable
