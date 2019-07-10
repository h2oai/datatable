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
#include "python/args.h"
#include "python/obj.h"
#include "utils/assert.h"
namespace py {



//------------------------------------------------------------------------------
// XTypeMaker
//------------------------------------------------------------------------------

/**
 * Helper class for initializing the `PyTypeObject type` variable.
 * See: https://docs.python.org/3/c-api/typeobj.html
 */
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
    static struct ReprTag {} repr_tag;
    static struct StrTag {} str_tag;
    static struct GetitemTag {} getitem_tag;
    static struct SetitemTag {} setitem_tag;

    XTypeMaker(PyTypeObject* t, size_t objsize) : type(t) {
      std::memset(type, 0, sizeof(PyTypeObject));
      Py_INCREF(type);
      type->tp_basicsize = static_cast<Py_ssize_t>(objsize);
      type->tp_itemsize = 0;
      type->tp_flags = Py_TPFLAGS_DEFAULT;
      type->tp_alloc = &PyType_GenericAlloc;
      type->tp_new   = &PyType_GenericNew;
    }

    void attach_to_module(PyObject* module) {
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
      xassert(meth_defs.size() == 0);
      xassert(type->tp_init == nullptr);
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

    void add(initproc _init, PKArgs& args, ConstructorTag) {
      args.set_class_name(type->tp_name);
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
      args.set_class_name(type->tp_name);
      meth_defs.push_back(PyMethodDef {
        args.get_short_name(),
        reinterpret_cast<PyCFunction>(meth),
        METH_VARARGS | METH_KEYWORDS,
        args.get_docstring()
      });
    }

    void add(reprfunc _repr, ReprTag) {
      type->tp_repr = _repr;
    }

    void add(reprfunc _str, StrTag) {
      type->tp_str = _str;
    }

    void add(binaryfunc _getitem, GetitemTag) {
      init_tp_as_mapping();
      type->tp_as_mapping->mp_subscript = _getitem;
    }

    void add(objobjargproc _setitem, SetitemTag) {
      init_tp_as_mapping();
      type->tp_as_mapping->mp_ass_subscript = _setitem;
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
      size_t n = meth_defs.size();
      PyMethodDef* res = new PyMethodDef[n + 1];
      std::memcpy(res, meth_defs.data(), n * sizeof(PyMethodDef));
      std::memset(res + n, 0, sizeof(PyMethodDef));
      return res;
    }

    void init_tp_as_mapping() {
      if (type->tp_as_mapping) return;
      type->tp_as_mapping = new PyMappingMethods;
      type->tp_as_mapping->mp_length = nullptr;
      type->tp_as_mapping->mp_subscript = nullptr;
      type->tp_as_mapping->mp_ass_subscript = nullptr;
    }

};




//------------------------------------------------------------------------------
// XObject
//------------------------------------------------------------------------------

template <typename Derived>
struct XObject : public PyObject {
  static PyTypeObject type;

  static void init_type(PyObject* module) {
    XTypeMaker xt(&type, sizeof(Derived));
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


template <typename D>
PyTypeObject XObject<D>::type;




//------------------------------------------------------------------------------
// Helper macros
//------------------------------------------------------------------------------

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-template"

template <typename T, typename R, typename ...Args>
static T class_of(R(T::*)(Args...));

using fn01_t = PyObject*(*)(PyObject*);
using fn11_t = PyObject*(*)(PyObject*, PyObject*);
using fn21_t = PyObject*(*)(PyObject*, PyObject*, PyObject*);
using fn20_t = void(PyObject*, PyObject*, PyObject*);

template <typename = int>
static PyObject* execute_and_catch_exceptions(PyObject* o, fn01_t fn) noexcept
{
  try {
    return fn(o);
  } catch (const std::exception& e) {
    exception_to_python(e);
    return nullptr;
  }
}

template <typename = int>
static PyObject* execute_and_catch_exceptions(
    PyObject* o, PyObject* arg, fn11_t fn) noexcept
{
  try {
    return fn(o, arg);
  } catch (const std::exception& e) {
    exception_to_python(e);
    return nullptr;
  }
}

template <typename = int>
static int execute_and_catch_exceptions(
    PyObject* o, PyObject* arg1, PyObject* arg2, fn20_t fn) noexcept
{
  try {
    fn(o, arg1, arg2);
    return 0;
  } catch (const std::exception& e) {
    exception_to_python(e);
    return -1;
  }
}


#define impl_WRAP01(METH)                                                      \
    [](PyObject* self) -> PyObject* {                                          \
      return execute_and_catch_exceptions(self,                                \
        [](PyObject* obj) -> PyObject* {                                       \
          using T = decltype(class_of(METH));                                  \
          return (static_cast<T*>(obj)->*METH)().release();                    \
        });                                                                    \
    }

#define impl_WRAP11(METH)                                                      \
    [](PyObject* self, PyObject* arg) -> PyObject* {                           \
      return execute_and_catch_exceptions(self, arg,                           \
        [](PyObject* obj, PyObject* a) -> PyObject* {                          \
          using T = decltype(class_of(METH));                                  \
          return (static_cast<T*>(obj)->*METH)(py::robj(a)).release();         \
        });                                                                    \
    }



#pragma clang diagnostic pop



#define CONSTRUCTOR(METH, ARGS)                                                \
    [](PyObject* self, PyObject* args, PyObject* kwargs) -> int {              \
      return ARGS.exec_intfn(self, args, kwargs,                               \
         [](PyObject* obj, const py::PKArgs& a){                               \
            (*reinterpret_cast<decltype(class_of(METH))*>(obj).*METH)(a);      \
         });                                                                   \
    }, ARGS, py::XTypeMaker::constructor_tag


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



#define METHOD__REPR__(METH)                                                   \
    impl_WRAP01(METH), py::XTypeMaker::repr_tag


#define METHOD__STR__(METH)                                                    \
    impl_WRAP01(METH), py::XTypeMaker::str_tag


#define METHOD__GETITEM__(METH)                                                \
    impl_WRAP11(METH), py::XTypeMaker::getitem_tag


#define METHOD__SETITEM__(METH)                                                \
    [](PyObject* self, PyObject* arg1, PyObject* arg2) -> int {                \
      return execute_and_catch_exceptions(self, arg1, arg2,                    \
        [](PyObject* obj, PyObject* a1, PyObject* a2) -> void {                \
          using T = decltype(class_of(METH));                                  \
          (static_cast<T*>(obj)->*METH)(py::robj(a1), py::robj(a2));           \
        });                                                                    \
    }, py::XTypeMaker::setitem_tag


} // namespace py
#endif
