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
#ifndef dt_PYTHON_EXT_TYPE_h
#define dt_PYTHON_EXT_TYPE_h
#include <vector>
#include <Python.h>
#include "python/args.h"
#include "python/obj.h"
#include "utils/exceptions.h"

namespace py {

/**
 * This functionality can be used in order to create new Python classes
 * from C++. In Python terminology they are called "extension types".
 *
 * This framework is the syntactic sugar over the "native" Python C API, as
 * described e.g. here: https://docs.python.org/3/extending/newtypes.html
 * You do not need to know the native API in order to use this framework.
 *
 * Here's a minimal example of how one might declare python class "Please":
 *
 *    class Please : public PyObject {
 *    public:
 *      struct Type : public py::ExtType<Please> {
 *        static const char* classname() { return "datatable.Please"; }
 *        static const char* classdoc();
 *      };
 *      ...
 *    };
 *
 * Then, in order to attach this class to a python module, call at the module
 * initialization stage:
 *
 *    Please::Type::init(module);
 *
 * Note that unlike standard Python C extension types, this initialization
 * method does not return a status code. Instead, should an error occur,
 * an exception will be thrown.
 *
 * Thus, in order to declare a python extension type, you should create a
 * class derived from `PyObject`, and containing a public internal class
 * `Type` deriving from `py::ExtType<YourClass>`. This internal class will
 * provide certain static methods which describe your main class to python.
 * The "main" class must not be virtual (i.e. it should not contain any
 * virtual methods) -- if not, the Python might lose its vtable part when
 * converting your object into a `PyObject*`.
 *
 * The methods that can be declared in class `Type` are:
 *
 *   static const char* classname()
 *      Return the name of the class (as a static string). This name should
 *      contain a proper module name. This is the only required method needed
 *      to create a functional extension type.
 *
 *   static const char* classdoc()
 *      Return the documentation text for the class. This should be provided
 *      for every user-facing class.
 *
 *   static PyTypeObject* baseclass()
 *      If this class derives from another class, it must be specified here.
 *      For example if `Please` was derived from `Request`, then this method
 *      should have returned `&Request::Type::type`.
 *
 *   static bool is_subclassable()
 *      Return true if the class allows to be derived from, or false if the
 *      class is final (default).
 *
 *   static void init_getsetters(GetSetters& gs)
 *      If the class supports getter / setter properties, then this method
 *      should be declare them. See more details below.
 *
 *   static void init_methods(Methods& mm)
 *      If the class support instance methods, then they must be declared
 *      here. See more details below.
 *
 *
 * In addition to these static methods, the main class itself can declare
 * certain special instance methods:
 *
 *   void m__init__(const py::PKArgs& args)
 *      This is the python-facing "constructor", the equivalent of pythonic
 *      `__init__(self, ...)`. Note that the argument to this function is a
 *      `py::PKArgs` instance which must be declared as a static variable
 *      inside the `Type` class and named `args___init__`.
 *
 *   void m__dealloc__()
 *      This is the python-facing "destructor". Its job is to release any
 *      resources that the class is currently holding.
 *
 *   oobj m__repr__()
 *      Return the stringified representation of the class. This is equivalent
 *      to pythonic `__repr__(self)` method.
 *
 *   void m__get_buffer__(Py_buffer* buf, int flags)
 *   void m__release_buffer__(Py_buffer* buf)
 *      These 2 methods, if present, signify that the object supports "Buffers
 *      protocol" (https://docs.python.org/3/c-api/buffer.html).
 *
 *
 * Getters / setters
 * -----------------
 * A property getter has generic signature `oobj (T::*)() const`, and a setter
 * has signature `void (T::*)(robj)`. These methods should be implemented in
 * your main class, and then declared in the static
 * `Type::init_methods_and_getsets(Methods&, GetSetters&)` function.
 *
 * For example, in order to define property "pretty" for our class `Please`,
 * we would write the following:
 *
 *    class Please : public PyObject {
 *      public:
 *        oobj get_pretty() const;
 *        void set_pretty(robj);
 *    };
 *
 *    static GSArgs args_pretty(
 *      "pretty",
 *      "True for 'pretty please', or False otherwise");
 *
 *    void Please::Type::init_methods_and_getsets(Methods&, GetSetters& gs) {
 *      ADD_GETSET(gs, &Please::get_pretty, &Please::set_pretty, args_pretty);
 *    }
 *
 *
 * Methods
 * -------
 * It is common for an extension object to define multiple methods that act
 * on that object and return some results. These methods are allowed to have
 * one of the followingsignatures:
 *
 *     oobj (T::*)(const PKArgs&);
 *     void (T::*)(const PKArgs&);
 *
 * where the "PKArgs" parameter encapsulates python-style positional and
 * keyword arguments. These methods must also be declared in the static
 * `Type::init_methods_and_getsets(Methods&, GetSetters&)` procedure.
 * For example:
 *
 *    class Please : public PyObject {
 *      public:
 *        oobj say(const PKArgs&);
 *    };
 *
 *    static PKArgs args_say(...);
 *    oobj Please::say(const PKArgs&) { ... }
 *
 *    void Please::Type::init_methods_and_getsets(Methods& mm, GetSetters&) {
 *      ADD_METHOD(mm, &Please::say, args_say);
 *    }
 *
 */
template <class T>
struct ExtType {
  static PyTypeObject type;

  static void init(PyObject* module);

  static const char* classdoc() { return nullptr; }
  static PyTypeObject* baseclass() { return nullptr; }
  static bool is_subclassable() { return false; }

  class GetSetters;
  class Methods;
};




//------------------------------------------------------------------------------
// Implementation internals
//------------------------------------------------------------------------------
namespace _impl {

  template <typename T>
  int _safe_init(PyObject* self, PyObject* args, PyObject* kwds) {
    try {
      T::Type::args___init__.bind(args, kwds);
      static_cast<T*>(self)->m__init__(T::Type::args___init__);
      return 0;
    } catch (const std::exception& e) {
      exception_to_python(e);
      return -1;
    }
  }

  template <typename T>
  PyObject* _safe_repr(PyObject* self) {
    try {
      oobj res = static_cast<T*>(self)->m__repr__();
      return std::move(res).release();
    } catch (const std::exception& e) {
      exception_to_python(e);
      return nullptr;
    }
  }

  template <typename T>
  void _safe_dealloc(PyObject* self) {
    try {
      T* tself = static_cast<T*>(self);
      tself->m__dealloc__();
      Py_TYPE(self)->tp_free(self);
    } catch (const std::exception& e) {
      exception_to_python(e);
    }
  }

  template <typename T>
  PyObject* _safe_getitem(PyObject* self, PyObject* key) {
    try {
      T* tself = static_cast<T*>(self);
      py::oobj res = tself->m__getitem__(py::robj(key));
      return std::move(res).release();
    } catch (const std::exception& e) {
      exception_to_python(e);
      return nullptr;
    }
  }

  template <typename T>
  int _safe_setitem(PyObject* self, PyObject* key, PyObject* val) {
    try {
      T* tself = static_cast<T*>(self);
      tself->m__setitem__(py::robj(key), py::robj(val));
      return 0;
    } catch (const std::exception& e) {
      exception_to_python(e);
      return -1;
    }
  }

  template <typename T>
  int _safe_getbuffer(PyObject* self, Py_buffer* buf, int flags) {
    try {
      T* tself = static_cast<T*>(self);
      tself->m__get_buffer__(buf, flags);
      return 0;
    } catch (const std::exception& e) {
      exception_to_python(e);
      return -1;
    }
  }

  template <typename T>
  void _safe_releasebuffer(PyObject* self, Py_buffer* buf) {
    try {
      T* tself = static_cast<T*>(self);
      tself->m__release_buffer__(buf);
    } catch (const std::exception& e) {
      exception_to_python(e);
    }
  }


  /**
   * SFINAE checker for the presence of various methods:
   *   has<T>::buffers  will return true if T has method `m__get_buffer__`
   *                    (signature is not verified)
   *   has<T>::_init_   returns true if T has method `m__init__`
   *   has<T>::_repr_   returns true if T has method `m__repr__`
   *   has<T>::dealloc  returns true if T has method `m__dealloc__`
   *   has<T>::mthgtst  returns true if T::Type has `init_methods_and_getsets`
   *   has<T>::getitem  returns true if T has method `m__getitem__`
   *   has<T>::setitem  returns true if T has method `m__setitem__`
   */
  template <typename T>
  class has {
    template <class C> static char test_init(decltype(&C::m__init__));
    template <class C> static long test_init(...);
    template <class C> static char test_repr(decltype(&C::m__repr__));
    template <class C> static long test_repr(...);
    template <class C> static char test_dealloc(decltype(&C::m__dealloc__));
    template <class C> static long test_dealloc(...);
    template <class C> static char test_buf(decltype(&C::m__get_buffer__));
    template <class C> static long test_buf(...);
    template <class C> static char test_mgs(decltype(&C::Type::init_methods_and_getsets));
    template <class C> static long test_mgs(...);
    template <class C> static char test_getitem(decltype(&C::m__getitem__));
    template <class C> static long test_getitem(...);
    template <class C> static char test_setitem(decltype(&C::m__setitem__));
    template <class C> static long test_setitem(...);

  public:
    static constexpr bool _init_  = (sizeof(test_init<T>(nullptr)) == 1);
    static constexpr bool _repr_  = (sizeof(test_repr<T>(nullptr)) == 1);
    static constexpr bool dealloc = (sizeof(test_dealloc<T>(nullptr)) == 1);
    static constexpr bool buffers = (sizeof(test_buf<T>(nullptr)) == 1);
    static constexpr bool mthgtst = (sizeof(test_mgs<T>(nullptr)) == 1);
    static constexpr bool getitem = (sizeof(test_getitem<T>(nullptr)) == 1);
    static constexpr bool setitem = (sizeof(test_setitem<T>(nullptr)) == 1);
  };

  /**
   * Conditional initialization of additional `type` parameters. Use this
   * class as follows:
   *
   *   init<T, has<T>::feature>::feature()
   *
   * This will initialize `<feature>`, but only if that feature actuall exists
   * in class `T`.
   */
  template <typename, bool>
  struct init {
    static void _init_(PyTypeObject&) {}
    static void _repr_(PyTypeObject&) {}
    static void buffers(PyTypeObject&) {}
    static void methods_and_getsets(PyTypeObject&) {}
    static void getitem(PyTypeObject&) {}
    static void setitem(PyTypeObject&) {}
  };

  template <typename T>
  struct init<T, true> {
    static void _init_(PyTypeObject& type) {
      T::Type::args___init__.set_class_name(T::Type::classname());
      type.tp_init = _safe_init<T>;
    }

    static void _repr_(PyTypeObject& type) {
      type.tp_repr = _safe_repr<T>;
    }

    static void dealloc(PyTypeObject& type) {
      type.tp_dealloc = _safe_dealloc<T>;
    }

    static void buffers(PyTypeObject& type) {
      PyBufferProcs* bufs = new PyBufferProcs();
      bufs->bf_getbuffer = _safe_getbuffer<T>;
      bufs->bf_releasebuffer = _safe_releasebuffer<T>;
      type.tp_as_buffer = bufs;
    }

    static void methods_and_getsets(PyTypeObject& type) {
      typename T::Type::GetSetters gs;
      typename T::Type::Methods mm;
      T::Type::init_methods_and_getsets(mm, gs);
      if (mm) type.tp_methods = mm.finalize();
      if (gs) type.tp_getset = gs.finalize();
    }

    static void getitem(PyTypeObject& type) {
      init_tp_as_mapping(type);
      type.tp_as_mapping->mp_subscript = _safe_getitem<T>;
    }

    static void setitem(PyTypeObject& type) {
      init_tp_as_mapping(type);
      type.tp_as_mapping->mp_ass_subscript = _safe_setitem<T>;
    }

    private:
      static void init_tp_as_mapping(PyTypeObject& type) {
        if (type.tp_as_mapping) return;
        type.tp_as_mapping = new PyMappingMethods;
        type.tp_as_mapping->mp_length = nullptr;
        type.tp_as_mapping->mp_subscript = nullptr;
        type.tp_as_mapping->mp_ass_subscript = nullptr;
      }
  };

}  // namespace _impl



//------------------------------------------------------------------------------
// Implementation
//------------------------------------------------------------------------------

template <class T>
PyTypeObject ExtType<T>::type;


/**
 * Main method for initializing the `PyTypeObject type` variable.
 * See: https://docs.python.org/3/c-api/typeobj.html
 */
template <class T>
void ExtType<T>::init(PyObject* module) {
  std::memset(&type, 0, sizeof(PyTypeObject));
  Py_INCREF(&type);

  type.tp_name      = T::Type::classname();
  type.tp_doc       = T::Type::classdoc();
  type.tp_base      = T::Type::baseclass();
  type.tp_basicsize = static_cast<Py_ssize_t>(sizeof(T));
  type.tp_itemsize  = 0;
  type.tp_flags     = Py_TPFLAGS_DEFAULT;
  if (T::Type::is_subclassable()) type.tp_flags |= Py_TPFLAGS_BASETYPE;

  type.tp_alloc     = &PyType_GenericAlloc;
  type.tp_new       = &PyType_GenericNew;

  _impl::init<T, _impl::has<T>::_init_ >::_init_(type);
  _impl::init<T, _impl::has<T>::_repr_ >::_repr_(type);
  _impl::init<T, _impl::has<T>::dealloc>::dealloc(type);
  _impl::init<T, _impl::has<T>::buffers>::buffers(type);
  _impl::init<T, _impl::has<T>::mthgtst>::methods_and_getsets(type);
  _impl::init<T, _impl::has<T>::getitem>::getitem(type);
  _impl::init<T, _impl::has<T>::setitem>::setitem(type);

  // Finish type initialization
  int r = PyType_Ready(&type);
  if (r < 0) throw PyError();

  PyObject* pyobj_type = reinterpret_cast<PyObject*>(&type);
  const char* name = std::strrchr(type.tp_name, '.');
  if (name) name++; else name = type.tp_name;

  r = PyModule_AddObject(module, name, pyobj_type);
  if (r < 0) throw PyError();
}




//------------------------------------------------------------------------------
// GetSetters
//------------------------------------------------------------------------------

template <class T>
class ExtType<T>::GetSetters {
  private:
    std::vector<PyGetSetDef> defs;

  public:
    void add(getter func, GSArgs& args) {
      defs.push_back(PyGetSetDef {
        const_cast<char*>(args.name),
        func, nullptr,
        const_cast<char*>(args.doc),
        nullptr  // closure
      });
    }

    void add(getter gfunc, setter sfunc, GSArgs& args) {
      defs.push_back(PyGetSetDef {
        const_cast<char*>(args.name),
        gfunc,
        sfunc,
        const_cast<char*>(args.doc),
        nullptr  // closure
      });
    }

    explicit operator bool() const {
      return !defs.empty();
    }

    PyGetSetDef* finalize() {
      PyGetSetDef* res = new PyGetSetDef[1 + defs.size()]();
      std::memcpy(res, defs.data(), defs.size() * sizeof(PyGetSetDef));
      return res;
    }
};


#define ADD_GETTER(GS, GETTER, ARGS)                                           \
  GS.add(                                                                      \
    [](PyObject* self, void*) -> PyObject* {                                   \
      return ARGS.exec_getter(self, GETTER);                                   \
    }, ARGS);                                                                  \

#define ADD_GETSET(GS, GETTER, SETTER, ARGS)                                   \
  GS.add(                                                                      \
    [](PyObject* self, void*) -> PyObject* {                                   \
      return ARGS.exec_getter(self, GETTER);                                   \
    },                                                                         \
    [](PyObject* self, PyObject* value, void*) -> int {                        \
      return ARGS.exec_setter(self, value, SETTER);                            \
    },                                                                         \
    ARGS);                                                                     \




//------------------------------------------------------------------------------
// Methods
//------------------------------------------------------------------------------

template <class T>
class ExtType<T>::Methods {
  private:
    std::vector<PyMethodDef> defs;

  public:
    void add(PyCFunctionWithKeywords func, PKArgs& args) {
      args.set_class_name(T::Type::classname());
      defs.push_back(PyMethodDef {
        args.get_short_name(),
        reinterpret_cast<PyCFunction>(func),
        METH_VARARGS | METH_KEYWORDS,
        args.get_docstring()
      });
    }

    explicit operator bool() const {
      return !defs.empty();
    }

    PyMethodDef* finalize() {
      PyMethodDef* res = new PyMethodDef[1 + defs.size()]();
      std::memcpy(res, defs.data(), defs.size() * sizeof(PyMethodDef));
      return res;
    }
};


#define ADD_METHOD(MM, METHOD, ARGS)                                           \
  MM.add(                                                                      \
    [](PyObject* self, PyObject* args, PyObject* kwds) -> PyObject* {          \
      return ARGS.exec_method(self, args, kwds, METHOD);                       \
    }, ARGS)                                                                   \



}  // namespace py
#endif
