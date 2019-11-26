//------------------------------------------------------------------------------
// Copyright 2019 H2O.ai
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//------------------------------------------------------------------------------
#include "python/xobject.h"

namespace py {

XTypeMaker::ConstructorTag XTypeMaker::constructor_tag;
XTypeMaker::DestructorTag XTypeMaker::destructor_tag;
XTypeMaker::GetSetTag XTypeMaker::getset_tag;
XTypeMaker::MethodTag XTypeMaker::method_tag;
XTypeMaker::Method0Tag XTypeMaker::method0_tag;
XTypeMaker::ReprTag XTypeMaker::repr_tag;
XTypeMaker::StrTag XTypeMaker::str_tag;
XTypeMaker::GetitemTag XTypeMaker::getitem_tag;
XTypeMaker::SetitemTag XTypeMaker::setitem_tag;
XTypeMaker::BuffersTag XTypeMaker::buffers_tag;
XTypeMaker::IterTag XTypeMaker::iter_tag;
XTypeMaker::NextTag XTypeMaker::next_tag;


XTypeMaker::XTypeMaker(PyTypeObject* t, size_t objsize) : type(t) {
  std::memset(type, 0, sizeof(PyTypeObject));
  Py_INCREF(type);
  type->tp_basicsize = static_cast<Py_ssize_t>(objsize);
  type->tp_itemsize = 0;
  type->tp_flags = Py_TPFLAGS_DEFAULT;
  type->tp_alloc = &PyType_GenericAlloc;
  type->tp_new   = &PyType_GenericNew;
}


void XTypeMaker::attach_to_module(PyObject* module) {
  xassert(type->tp_dealloc);
  xassert(type->tp_init);
  xassert(type->tp_name);

  if (get_defs.size()) {
    type->tp_getset = finalize_getsets();
  }
  if (meth_defs.size()) {
    type->tp_methods = finalize_methods();
  }

  int r = PyType_Ready(type);
  if (r < 0) throw PyError();

  if (module) {
    const char* dot = std::strrchr(type->tp_name, '.');
    const char* name = dot? dot + 1 : type->tp_name;
    r = PyModule_AddObject(module, name, reinterpret_cast<PyObject*>(type));
    if (r < 0) throw PyError();
  }
}


void XTypeMaker::set_class_name(const char* name) {
  xassert(meth_defs.size() == 0);
  xassert(type->tp_init == nullptr);
  type->tp_name = name;
}


void XTypeMaker::set_class_doc(const char* doc) {
  type->tp_doc = doc;
}


void XTypeMaker::set_base_class(PyTypeObject* base_type) {
  type->tp_base = base_type;
}


void XTypeMaker::set_subclassable(bool flag /* = true */) {
  if (flag) {
    type->tp_flags |= Py_TPFLAGS_BASETYPE;
  } else {
    type->tp_flags &= ~Py_TPFLAGS_BASETYPE;
  }
}


// initproc = int(*)(PyObject*, PyObject*, PyObject*)
void XTypeMaker::add(initproc _init, PKArgs& args, ConstructorTag) {
  args.set_class_name(type->tp_name);
  type->tp_init = _init;
}


// destructor = void(*)(PyObject*)
void XTypeMaker::add(destructor _dealloc, DestructorTag) {
  type->tp_dealloc = _dealloc;
}

// getter = PyObject*(*)(PyObject*, void*)

// setter = int(*)(PyObject*, PyObject*, void*)
void XTypeMaker::add(getter getfunc, setter setfunc, GSArgs& args, GetSetTag) {
  get_defs.push_back(PyGetSetDef {
    const_cast<char*>(args.name),
    getfunc, setfunc,
    const_cast<char*>(args.doc),
    nullptr  // closure
  });
}


// PyCFunctionWithKeywords = PyObject*(*)(PyObject*, PyObject*, PyObject*)
void XTypeMaker::add(PyCFunctionWithKeywords meth, PKArgs& args, MethodTag) {
  args.set_class_name(type->tp_name);
  meth_defs.push_back(PyMethodDef {
    args.get_short_name(),
    reinterpret_cast<PyCFunction>(meth),
    METH_VARARGS | METH_KEYWORDS,
    args.get_docstring()
  });
}


// unaryfunc = PyObject*(*)(PyObject*)
void XTypeMaker::add(unaryfunc meth, const char* name, Method0Tag) {
  meth_defs.push_back(PyMethodDef {
    name, reinterpret_cast<PyCFunction>(meth),
    METH_NOARGS, nullptr
  });
}


// reprfunc = PyObject*(*)(PyObject*)
void XTypeMaker::add(reprfunc _repr, ReprTag) {
  type->tp_repr = _repr;
}


// reprfunc = PyObject*(*)(PyObject*)
void XTypeMaker::add(reprfunc _str, StrTag) {
  type->tp_str = _str;
}


// binaryfunc = PyObject*(*)(PyObject*, PyObject*)
void XTypeMaker::add(binaryfunc _getitem, GetitemTag) {
  init_tp_as_mapping();
  type->tp_as_mapping->mp_subscript = _getitem;
}


// objobjargproc = int(*)(PyObject*, PyObject*, PyObject*)
void XTypeMaker::add(objobjargproc _setitem, SetitemTag) {
  init_tp_as_mapping();
  type->tp_as_mapping->mp_ass_subscript = _setitem;
}

// getbufferproc = int(*)(PyObject*, Py_buffer*, int)

// releasebufferproc = void(*)(PyObject*, Py_buffer*)
void XTypeMaker::add(getbufferproc _get, releasebufferproc _del, BuffersTag) {
  xassert(type->tp_as_buffer == nullptr);
  PyBufferProcs* bufs = new PyBufferProcs();
  bufs->bf_getbuffer = _get;
  bufs->bf_releasebuffer = _del;
  type->tp_as_buffer = bufs;
}


// getiterfunc = PyObject*(*)(PyObject*)
void XTypeMaker::add(getiterfunc _iter, IterTag) {
  type->tp_iter = _iter;
}


// iternextfunc = PyObject*(*)(PyObject*)
void XTypeMaker::add(iternextfunc _next, NextTag) {
  if (!type->tp_iter) {
    type->tp_iter = PyObject_SelfIter;
  }
  type->tp_iternext = _next;
}


PyGetSetDef* XTypeMaker::finalize_getsets() {
  size_t n = get_defs.size();
  PyGetSetDef* res = new PyGetSetDef[n + 1];
  std::memcpy(res, get_defs.data(), n * sizeof(PyGetSetDef));
  std::memset(res + n, 0, sizeof(PyGetSetDef));
  return res;
}


PyMethodDef* XTypeMaker::finalize_methods() {
  size_t n = meth_defs.size();
  PyMethodDef* res = new PyMethodDef[n + 1];
  std::memcpy(res, meth_defs.data(), n * sizeof(PyMethodDef));
  std::memset(res + n, 0, sizeof(PyMethodDef));
  return res;
}


void XTypeMaker::init_tp_as_mapping() {
  if (type->tp_as_mapping) return;
  type->tp_as_mapping = new PyMappingMethods;
  type->tp_as_mapping->mp_length = nullptr;
  type->tp_as_mapping->mp_subscript = nullptr;
  type->tp_as_mapping->mp_ass_subscript = nullptr;
}

} // namespace py
