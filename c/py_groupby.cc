//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#define dt_PY_GROUPBY_cc
#include "py_groupby.h"
#include "python/int.h"
#include "python/list.h"

namespace pygroupby
{


/**
 * Create a new pygroupby object by wrapping the provided Groupby.
 * The returned py-object will hold a shallow copy of the source.
 */
PyObject* wrap(const Groupby& grpby) {
  PyObject* pytype = reinterpret_cast<PyObject*>(&pygroupby::type);
  PyObject* pygby = PyObject_CallObject(pytype, nullptr);
  if (pygby) {
    auto pypygb = static_cast<pygroupby::obj*>(pygby);
    pypygb->ref = new Groupby(grpby);
  }
  return pygby;
}

Groupby* unwrap(PyObject* object) {
  if (!object) throw PyError();
  if (object == Py_None) return nullptr;
  if (!PyObject_TypeCheck(object, &pygroupby::type)) {
    throw TypeError() << "Expected object of type Groupby";
  }
  return static_cast<pygroupby::obj*>(object)->ref;
}



//==============================================================================
// Getters/setters
//==============================================================================

PyObject* get_ngroups(obj* self) {
  return PyLong_FromSize_t(self->ref->ngroups());
}

PyObject* get_group_sizes(obj* self) {
  Groupby* grpby = self->ref;
  size_t ng = grpby->ngroups();
  const int32_t* offsets = grpby->offsets_r();
  py::olist res(ng);
  for (size_t i = 0; i < ng; ++i) {
    res.set(i, py::oInt(offsets[i + 1] - offsets[i]));
  }
  return res.release();
}

PyObject* get_group_offsets(obj* self) {
  Groupby* grpby = self->ref;
  size_t ng = grpby->ngroups();
  const int32_t* offsets = grpby->offsets_r();
  py::olist res(ng + 1);
  for (size_t i = 0; i <= ng; ++i) {
    res.set(i, py::oInt(offsets[i]));
  }
  return res.release();
}



//==============================================================================
// Methods
//==============================================================================

static void dealloc(obj* self) {
  delete self->ref;
  self->ref = nullptr;
  Py_TYPE(self)->tp_free(self);
}



//==============================================================================
// DataTable type definition
//==============================================================================

static PyGetSetDef groupby_getsetters[] = {
  GETTER(ngroups),
  GETTER(group_sizes),
  GETTER(group_offsets),
  {nullptr, nullptr, nullptr, nullptr, nullptr}  /* sentinel */
};


PyTypeObject type = {
  PyVarObject_HEAD_INIT(nullptr, 0)
  "datatable.core.Groupby",           /* tp_name */
  sizeof(obj),                        /* tp_basicsize */
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
  nullptr,                            /* tp_as_buffer */
  Py_TPFLAGS_DEFAULT,                 /* tp_flags */
  nullptr,                            /* tp_doc */
  nullptr,                            /* tp_traverse */
  nullptr,                            /* tp_clear */
  nullptr,                            /* tp_richcompare */
  0,                                  /* tp_weaklistoffset */
  nullptr,                            /* tp_iter */
  nullptr,                            /* tp_iternext */
  nullptr,                            /* tp_methods */
  nullptr,                            /* tp_members */
  groupby_getsetters,                 /* tp_getset */
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


// Add PyRowIndex object to the Python module
int static_init(PyObject *module) {
  type.tp_new = PyType_GenericNew;
  if (PyType_Ready(&type) < 0) return 0;
  PyObject* typeobj = reinterpret_cast<PyObject*>(&type);
  Py_INCREF(typeobj);
  PyModule_AddObject(module, "Groupby", typeobj);
  return 1;
}


};  // namespace pygroupby
