//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
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
      PyGetSetDef* finalize();
  };

  class Methods {
    std::vector<PyMethodDef> defs;
    public:
      template <oobj (T::*F)(NoArgs&),        NoArgs& ARGS>
      void add(const char* name, const char* doc = nullptr);
      template <oobj (T::*F)(PKArgs&), PKArgs& ARGS>
      void add(const char* name, const char* doc = nullptr);
      template <void (T::*F)(NoArgs&),        NoArgs& ARGS>
      void add(const char* name, const char* doc = nullptr);
      template <void (T::*F)(PKArgs&), PKArgs& ARGS>
      void add(const char* name, const char* doc = nullptr);
      PyMethodDef* finalize();
    private:
      template <typename A, oobj (T::*F)(A&), A& ARGS>
      void add(const char* name, const char* doc = nullptr);
      template <typename A, void (T::*F)(A&), A& ARGS>
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
      return res.release();
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
      return res.release();
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

  template <typename T, typename A, oobj (T::*F)(A&), A& ARGS>
  PyObject* _safe_method1(PyObject* self, PyObject* args, PyObject* kwds) {
    try {
      T* tself = static_cast<T*>(self);
      ARGS.bind(args, kwds);
      oobj res = (tself->*F)(ARGS);
      return res.release();
    } catch (const std::exception& e) {
      exception_to_python(e);
      return nullptr;
    }
  }

  template <typename T, typename A, void (T::*F)(A&), A& ARGS>
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
   *   has<T>::getsets  returns true if T::Type has `init_getsettes`
   *   has<T>::meth     returns true if T::Type has `init_methods`
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
    template <class C> static char test_gs(decltype(&C::Type::init_getsetters));
    template <class C> static long test_gs(...);
    template <class C> static char test_meth(decltype(&C::Type::init_methods));
    template <class C> static long test_meth(...);

  public:
    static constexpr bool _init_  = (sizeof(test_init<T>(nullptr)) == 1);
    static constexpr bool _repr_  = (sizeof(test_repr<T>(nullptr)) == 1);
    static constexpr bool dealloc = (sizeof(test_dealloc<T>(nullptr)) == 1);
    static constexpr bool buffers = (sizeof(test_buf<T>(nullptr)) == 1);
    static constexpr bool getsets = (sizeof(test_gs<T>(nullptr)) == 1);
    static constexpr bool methods = (sizeof(test_meth<T>(nullptr)) == 1);
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
    static void getsets(PyTypeObject&) {}
    static void methods(PyTypeObject&) {}
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

    static void getsets(PyTypeObject& type) {
      typename T::Type::GetSetters gs;
      T::Type::init_getsetters(gs);
      type.tp_getset = gs.finalize();
    }

    static void methods(PyTypeObject& type) {
      typename T::Type::Methods mm;
      T::Type::init_methods(mm);
      type.tp_methods = mm.finalize();
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

  _impl::init<T, _impl::has<T>::_init_>::_init_(type);
  _impl::init<T, _impl::has<T>::_repr_>::_repr_(type);
  _impl::init<T, _impl::has<T>::dealloc>::dealloc(type);
  _impl::init<T, _impl::has<T>::buffers>::buffers(type);
  _impl::init<T, _impl::has<T>::getsets>::getsets(type);
  _impl::init<T, _impl::has<T>::methods>::methods(type);

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
template <oobj (T::*f)() const>
void ExtType<T>::GetSetters::add(const char* name, const char* doc) {
  defs.push_back(PyGetSetDef {
    const_cast<char*>(name),
    &_impl::_safe_getter<T, f>,
    nullptr,
    const_cast<char*>(doc),
    nullptr  // closure
  });
}

template <class T>
template <oobj (T::*getter)() const, void (T::*setter)(obj)>
void ExtType<T>::GetSetters::add(const char* name, const char* doc) {
  defs.push_back(PyGetSetDef {
    const_cast<char*>(name),
    &_impl::_safe_getter<T, getter>,
    &_impl::_safe_setter<T, setter>,
    const_cast<char*>(doc),
    nullptr  // closure
  });
}

template <class T>
PyGetSetDef* ExtType<T>::GetSetters::finalize() {
  PyGetSetDef* res = new PyGetSetDef[1 + defs.size()]();
  std::memcpy(res, defs.data(), defs.size() * sizeof(PyGetSetDef));
  return res;
}


//---- Methods ----

template <class T>
template <typename A, oobj (T::*F)(A&), A& ARGS>
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
template <typename A, void (T::*F)(A&), A& ARGS>
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
template <oobj (T::*F)(NoArgs&), NoArgs& ARGS>
void ExtType<T>::Methods::add(const char* name, const char* doc) {
  add<NoArgs, F, ARGS>(name, doc);
}

template <class T>
template <oobj (T::*F)(PKArgs&), PKArgs& ARGS>
void ExtType<T>::Methods::add(const char* name, const char* doc) {
  add<PKArgs, F, ARGS>(name, doc);
}

template <class T>
template <void (T::*F)(NoArgs&), NoArgs& ARGS>
void ExtType<T>::Methods::add(const char* name, const char* doc) {
  add<NoArgs, F, ARGS>(name, doc);
}

template <class T>
template <void (T::*F)(PKArgs&), PKArgs& ARGS>
void ExtType<T>::Methods::add(const char* name, const char* doc) {
  add<PKArgs, F, ARGS>(name, doc);
}

template <class T>
PyMethodDef* ExtType<T>::Methods::finalize() {
  PyMethodDef* res = new PyMethodDef[1 + defs.size()]();
  std::memcpy(res, defs.data(), defs.size() * sizeof(PyMethodDef));
  return res;
}


}  // namespace py

#endif
