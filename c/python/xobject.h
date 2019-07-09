//------------------------------------------------------------------------------
// Copyright 2018 H2O.ai
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
#ifndef dt_PYTHON_XOBJECT_h
#define dt_PYTHON_XOBJECT_h
#include <cstring>    // std::memcpy, std::memset, std::strrchr
#include <vector>     // std::vector
#include <Python.h>
#include "python/obj.h"
#include "utils/assert.h"
namespace py {


//------------------------------------------------------------------------------
// Helper macros
//------------------------------------------------------------------------------

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-template"

template <typename T>
static T class_of(oobj(T::*)(const PKArgs&));

template <typename T>
static T class_of(void(T::*)(const PKArgs&));

template <typename T>
static T class_of(void(T::*)());

#pragma clang diagnostic pop



#define CONSTRUCTOR(METH, ARGS)                                                \
    [](PyObject* self, PyObject* args, PyObject* kwargs) -> int {              \
      return ARGS.exec_intfn(self, args, kwargs,                               \
         [](PyObject* obj, const py::PKArgs& a){                               \
            (*reinterpret_cast<decltype(class_of(METH))*>(obj).*METH)(a);      \
         });                                                                   \
    }, py::XTypeMaker::constructor_tag


#define DESTRUCTOR(METH)                                                       \
    [](PyObject* self) -> void {                                               \
      (*reinterpret_cast<decltype(class_of(METH))*>(self).*METH)();            \
    }, py::XTypeMaker::destructor_tag


#define GETTER(GETFN, ARGS)                                                    \
    [](PyObject* self, void*) -> PyObject* {                                   \
      return ARGS.exec_getter(self, GETFN);                                    \
    }, nullptr, ARGS, py::XTypeMaker::getset_tag


#define GETSET(GETFN, SETFN, ARGS)                                             \
    [](PyObject* self, void*) -> PyObject* {                                   \
      return ARGS.exec_getter(self, GETFN);                                    \
    },                                                                         \
    [](PyObject* self, PyObject* value, void*) -> int {                        \
      return ARGS.exec_setter(self, value, SETFN);                             \
    }, ARGS, py::XTypeMaker::getset_tag


#define METHOD(METH, ARGS)                                                     \
    [](PyObject* self, PyObject* args, PyObject* kwds) -> PyObject* {          \
      return ARGS.exec_method(self, args, kwds, METH);                         \
    }, ARGS, py::XTypeMaker::method_tag




//------------------------------------------------------------------------------
// XTypeMaker
//------------------------------------------------------------------------------

// Helper class for initializing a PyTypeObject
class XTypeMaker {
  private:
    PyTypeObject* type;
    std::vector<PyGetSetDef> get_defs;
    std::vector<PyMethodDef> meth_defs;

  public:
    static struct ConstructorTag {} constructor_tag;
    static struct DestructorTag {} destructor_tag;
    static struct GetSetTag {} getset_tag;
    static struct MethodTag {} method_tag;

    XTypeMaker(PyTypeObject* t) : type(t) {
      std::memset(type, 0, sizeof(PyTypeObject));
      Py_INCREF(type);
      type->tp_flags = Py_TPFLAGS_DEFAULT;
      type->tp_alloc = &PyType_GenericAlloc;
      type->tp_new   = &PyType_GenericNew;
    }

    void attach_to_module(PyObject* module) {
      xassert(type->tp_basicsize > 0);
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

      const char* dot = std::strrchr(type->tp_name, '.');
      const char* name = dot? dot + 1 : type->tp_name;

      r = PyModule_AddObject(module, name, reinterpret_cast<PyObject*>(type));
      if (r < 0) throw PyError();
    }

    void set_class_name(const char* name) {
      type->tp_name = name;
    }

    void set_class_doc(const char* doc) {
      type->tp_doc = doc;
    }

    void set_base_class(PyTypeObject* base_type) {
      type->tp_base = base_type;
    }

    void set_subclassable(bool flag = true) {
      if (flag) {
        type->tp_flags |= Py_TPFLAGS_BASETYPE;
      } else {
        type->tp_flags &= ~Py_TPFLAGS_BASETYPE;
      }
    }

    void set_object_size(size_t sz) {
      type->tp_basicsize = static_cast<Py_ssize_t>(sz);
      type->tp_itemsize = 0;
    }

    void add(initproc _init, ConstructorTag) {
      type->tp_init = _init;
    }

    void add(destructor _dealloc, DestructorTag) {
      type->tp_dealloc = _dealloc;
    }

    void add(getter getfunc, setter setfunc, GSArgs& args, GetSetTag) {
      get_defs.push_back(PyGetSetDef {
        const_cast<char*>(args.name),
        getfunc, setfunc,
        const_cast<char*>(args.doc),
        nullptr  // closure
      });
    }

    void add(PyCFunctionWithKeywords meth, PKArgs& args, MethodTag) {
      meth_defs.push_back(PyMethodDef {
        args.get_short_name(),
        reinterpret_cast<PyCFunction>(meth),
        METH_VARARGS | METH_KEYWORDS,
        args.get_docstring()
      });
    }

  private:
    PyGetSetDef* finalize_getsets() {
      size_t n = get_defs.size();
      PyGetSetDef* res = new PyGetSetDef[n + 1];
      std::memcpy(res, get_defs.data(), n * sizeof(PyGetSetDef));
      std::memset(res + n, 0, sizeof(PyGetSetDef));
      return res;
    }

    PyMethodDef* finalize_methods() {
      for (auto& meth_def : meth_defs) {
        meth_def.set_class_name(type->tp_name);
      }
      size_t n = meth_defs.size();
      PyMethodDef* res = new PyMethodDef[n + 1];
      std::memcpy(res, meth_defs.data(), n * sizeof(PyMethodDef));
      std::memset(res + n, 0, sizeof(PyMethodDef));
      return res;
    }

};




//------------------------------------------------------------------------------
// XObject
//------------------------------------------------------------------------------

template <class Derived>
struct XObject : public PyObject {
  static PyTypeObject type;

  static void init_type(PyObject* module) {
    XTypeMaker xt(&type);
    Derived::impl_init_type(xt);
    xt.attach_to_module(module);
  }

  static bool check(PyObject* v) {
    if (!v) return false;
    auto typeptr = reinterpret_cast<PyObject*>(&type);
    int ret = PyObject_IsInstance(v, typeptr);
    if (ret == -1) PyErr_Clear();
    return (ret == 1);
  }
};



template <class T>
PyTypeObject XObject<T>::type;


} // namespace py
#endif
