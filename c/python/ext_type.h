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
 * method does not return a status code. Instead, should an error occur, it
 * will be thrown as an exception.
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
 *   void m__init__(py::Args& args)
 *      This is the python-facing "constructor", the equivalent of pythonic
 *      `__init__(self, ...)`. Note that the argument to this function is a
 *      `py::Args` instance which must be declared as a static variable inside
 *      the `Type` class and named `args___init__`.
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
 * has signature `void (T::*)(obj)`. These methods should be implemented in
 * your main class, and then declared in `Type::init_getsetters(GetSetters&)`.
 * Inside the `init_getsetters(gs)` you should call
 * `gs.add<getter, setter>(name, doc)` for each property in your class. Some
 * properties may not have setters, and the documentation string is optional
 * too: `gs.add<getter>(name)`.
 *
 * For example, in order to define property "pretty" for our class `Please`,
 * we would write the following:
 *
 *    class Please : public PyObject {
 *    public:
 *      oobj get_pretty() const;
 *      void set_pretty(obj);
 *      ...
 *      struct Type : py::ExtType<Please> {
 *        ...
 *        static void init_getsetters(GetSetters& gs) {
 *          gs.add<&Please::get_pretty, &Please::set_pretty>("please",
 *            "True for 'pretty please', or False otherwise");
 *        }
 *      };
 *    };
 *
 *
 * Methods
 * -------
 * It is common for an extension object to define multiple methods that act
 * on that object and return some results. Generally, such object have
 * signatures `oobj (T::*)(Args&)`, where the "Args" parameter encapsulates
 * python-style positional and keyword arguments. All methods that you define
 * must be declared in the static `Type::init_methods(Methods&)` procedure.
 * For example:
 *
 *    class Please : public PyObject {
 *    public:
 *      oobj say(py::NoArgs&);
 *      ...
 *      struct Type : py::ExtType<Please> {
 *        static py::NoArgs args_say;
 *        ...
 *        static void init_methods(Methods& mm) {
 *          mm.add<&Please::say, args_say>("say", "Say 'please'...");
 *        }
 *      };
 *    };
 *    py::NoArgs Please::Type::args_say;
 *
 * The following method signatures are supported:
 *
 *    oobj meth1(NoArgs&);
 *    oobj meth2(PKArgs&);
 *    void meth3(NoArgs&);
 *    void meth4(PKArgs&);
 *
 * Note that for each method a dedicated static object of type `py::Args` must
 * be provided. This object at minimum stores the name of the method (so that
 * we can provide sensible error messages at runtime), but more generally the
 * complete function signature. See "python/args.h" for more details.
 *
 */
template <class T>
struct ExtType {
  static PyTypeObject type;

  static void init(PyObject* module);

  static const char* classdoc() { return nullptr; }
  static PyTypeObject* baseclass() { return nullptr; }
  static bool is_subclassable() { return false; }

  class GetSetters {
    std::vector<PyGetSetDef> defs;
    public:
      using getter = oobj (T::*)() const;
      using setter = void (T::*)(obj);
      template <getter fg>            void add(const char* name, const char* doc = nullptr);
      template <getter fg, setter fs> void add(const char* name, const char* doc = nullptr);
      explicit operator bool() const;
      PyGetSetDef* finalize();
  };

  class Methods {
    std::vector<PyMethodDef> defs;
    public:
      template <oobj (T::*F)(const NoArgs&), NoArgs& ARGS>
      void add(const char* name, const char* doc = nullptr);
      template <oobj (T::*F)(const PKArgs&), PKArgs& ARGS>
      void add(const char* name, const char* doc = nullptr);
      template <void (T::*F)(const NoArgs&), NoArgs& ARGS>
      void add(const char* name, const char* doc = nullptr);
      template <void (T::*F)(const PKArgs&), PKArgs& ARGS>
      void add(const char* name, const char* doc = nullptr);
      explicit operator bool() const;
      PyMethodDef* finalize();
    private:
      template <typename A, oobj (T::*F)(const A&), A& ARGS>
      void add(const char* name, const char* doc = nullptr);
      template <typename A, void (T::*F)(const A&), A& ARGS>
      void add(const char* name, const char* doc = nullptr);
  };
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
    } catch (const std::exception& e) {
      exception_to_python(e);
    }
  }

  template <typename T>
  PyObject* _safe_getitem(PyObject* self, PyObject* key) {
    try {
      const T* tself = static_cast<const T*>(self);
      py::oobj res = tself->m__getitem__(py::obj(key));
      return std::move(res).release();
    } catch (const std::exception& e) {
      exception_to_python(e);
      return nullptr;
    }
  }

  template <typename T>
  PyObject* _safe_setitem(PyObject* self, PyObject* key, PyObject* val) {
    try {
      T* tself = static_cast<T*>(self);
      tself->m__setitem__(py::obj(key), py::obj(val));
      Py_RETURN_NONE;
    } catch (const std::exception& e) {
      exception_to_python(e);
      return nullptr;
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

  template <typename T, oobj (T::*F)() const>
  PyObject* _safe_getter(PyObject* self, void*) {
    try {
      T* t = static_cast<T*>(self);
      oobj res = (t->*F)();
      return std::move(res).release();
    } catch (const std::exception& e) {
      exception_to_python(e);
      return nullptr;
    }
  }

  template <typename T, void (T::*F)(obj)>
  int _safe_setter(PyObject* self, PyObject* value, void*) {
    try {
      T* t = static_cast<T*>(self);
      (t->*F)(obj(value));
      return 0;
    } catch (const std::exception& e) {
      exception_to_python(e);
      return -1;
    }
  }

  template <typename T, typename A, oobj (T::*F)(const A&), A& ARGS>
  PyObject* _safe_method1(PyObject* self, PyObject* args, PyObject* kwds) {
    try {
      T* tself = static_cast<T*>(self);
      ARGS.bind(args, kwds);
      oobj res = (tself->*F)(ARGS);
      return std::move(res).release();
    } catch (const std::exception& e) {
      exception_to_python(e);
      return nullptr;
    }
  }

  template <typename T, typename A, void (T::*F)(const A&), A& ARGS>
  PyObject* _safe_method2(PyObject* self, PyObject* args, PyObject* kwds) {
    try {
      T* tself = static_cast<T*>(self);
      ARGS.bind(args, kwds);
      (tself->*F)(ARGS);
      Py_RETURN_NONE;
    } catch (const std::exception& e) {
      exception_to_python(e);
      return nullptr;
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
      T::Type::args___init__.set_function_name("__init__");
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


//---- GetSetters ----

template <class T>
template <oobj (T::*gg)() const>
void ExtType<T>::GetSetters::add(const char* name, const char* doc) {
  defs.push_back(PyGetSetDef {
    const_cast<char*>(name),
    &_impl::_safe_getter<T, gg>,
    nullptr,
    const_cast<char*>(doc),
    nullptr  // closure
  });
}

template <class T>
template <oobj (T::*gg)() const, void (T::*ss)(obj)>
void ExtType<T>::GetSetters::add(const char* name, const char* doc) {
  defs.push_back(PyGetSetDef {
    const_cast<char*>(name),
    &_impl::_safe_getter<T, gg>,
    &_impl::_safe_setter<T, ss>,
    const_cast<char*>(doc),
    nullptr  // closure
  });
}

template <class T>
ExtType<T>::GetSetters::operator bool() const {
  return !defs.empty();
}

template <class T>
PyGetSetDef* ExtType<T>::GetSetters::finalize() {
  PyGetSetDef* res = new PyGetSetDef[1 + defs.size()]();
  std::memcpy(res, defs.data(), defs.size() * sizeof(PyGetSetDef));
  return res;
}


//---- Methods ----

template <class T>
template <typename A, oobj (T::*F)(const A&), A& ARGS>
void ExtType<T>::Methods::add(const char* name, const char* doc) {
  ARGS.set_class_name(T::Type::classname());
  ARGS.set_function_name(name);
  defs.push_back(PyMethodDef {
    name,
    reinterpret_cast<PyCFunction>(&_impl::_safe_method1<T, A, F, ARGS>),
    METH_VARARGS | METH_KEYWORDS,
    doc
  });
}

template <class T>
template <typename A, void (T::*F)(const A&), A& ARGS>
void ExtType<T>::Methods::add(const char* name, const char* doc) {
  ARGS.set_class_name(T::Type::classname());
  ARGS.set_function_name(name);
  defs.push_back(PyMethodDef {
    name,
    reinterpret_cast<PyCFunction>(&_impl::_safe_method2<T, A, F, ARGS>),
    METH_VARARGS | METH_KEYWORDS,
    doc
  });
}

template <class T>
template <oobj (T::*F)(const NoArgs&), NoArgs& ARGS>
void ExtType<T>::Methods::add(const char* name, const char* doc) {
  add<NoArgs, F, ARGS>(name, doc);
}

template <class T>
template <oobj (T::*F)(const PKArgs&), PKArgs& ARGS>
void ExtType<T>::Methods::add(const char* name, const char* doc) {
  add<PKArgs, F, ARGS>(name, doc);
}

template <class T>
template <void (T::*F)(const NoArgs&), NoArgs& ARGS>
void ExtType<T>::Methods::add(const char* name, const char* doc) {
  add<NoArgs, F, ARGS>(name, doc);
}

template <class T>
template <void (T::*F)(const PKArgs&), PKArgs& ARGS>
void ExtType<T>::Methods::add(const char* name, const char* doc) {
  add<PKArgs, F, ARGS>(name, doc);
}

template <class T>
ExtType<T>::Methods::operator bool() const {
  return !defs.empty();
}

template <class T>
PyMethodDef* ExtType<T>::Methods::finalize() {
  PyMethodDef* res = new PyMethodDef[1 + defs.size()]();
  std::memcpy(res, defs.data(), defs.size() * sizeof(PyMethodDef));
  return res;
}


}  // namespace py

#endif
