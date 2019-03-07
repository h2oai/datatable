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
  nullptr,                            /* tp_methods */
  nullptr,                            /* tp_members */
  nullptr,                            /* tp_getset */
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
