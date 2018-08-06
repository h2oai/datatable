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
#include "utils/exceptions.h"

namespace py {

/**
 * Usage:
 *   class MyObj : public PyObject {
 *     public:
 *       class Type : public py::ExtType<MyObj> {
 *         static const char* classname();  // required
 *         static const char* classdoc();
 *         static PyTypeObject* baseclass();
 *       };
 *
 *       MyObj();
 *       void m__init__(Args);
 *       void m__dealloc__();
 *
 *       void init_methods();
 *       void init_getsetters();
 *   };
 *
 * Then, in datatablemodule.c:
 *   MyObj::Type::init(module);
 */

template <class T>
struct ExtType {
  static PyTypeObject type;

  static void init(PyObject* module);

  static const char* classdoc() { return nullptr; }
  static PyTypeObject* baseclass() { return nullptr; }
  static bool is_subclassable() { return false; }
};



//------------------------------------------------------------------------------
// Implementation internals
//------------------------------------------------------------------------------
namespace _impl {

  template <typename T>
  int _safe_init(PyObject* self, PyObject* args, PyObject* kwds) {
    try {
      T::args__init__.bind(args, kwds);
      static_cast<T*>(self)->m__init__(T::args__init__);
      return 0;
    } catch (const std::exception& e) {
      exception_to_python(e);
      return -1;
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

  /**
   * SFINAE checker for the presence of various methods:
   *   has<T>::buffers  will return true if T has method `m__get_buffer__`
   *                    (signature is not verified)
   *   has<T>::init returns true if T has method `m__init__`
   */
  template <typename T>
  class has {
    template <class C> static char test_init(decltype(&C::m__init__));
    template <class C> static long test_init(...);
    template <class C> static char test_buf(decltype(&C::m__get_buffer__));
    template <class C> static long test_buf(...);

  public:
    static constexpr bool _init_ = (sizeof(test_init<T>(nullptr)) == 1);
    static constexpr bool buffers = (sizeof(test_buf<T>(nullptr)) == 1);
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
    static void buffers(PyTypeObject&) {}
  };

  template <typename T>
  struct init<T, true> {
    static void _init_(PyTypeObject& type) {
      type.tp_init = _safe_init<T>;
    }

    static void buffers(PyTypeObject& type) {
      PyBufferProcs* bufs = new PyBufferProcs();
      bufs->bf_getbuffer = _safe_getbuffer<T>;
      bufs->bf_releasebuffer = _safe_releasebuffer<T>;
      type.tp_as_buffer = bufs;
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
  type.tp_dealloc   = _impl::_safe_dealloc<T>;

  _impl::init<T, _impl::has<T>::_init_>::_init_(type);
  _impl::init<T, _impl::has<T>::buffers>::buffers(type);

  // Finish type initialization
  int r = PyType_Ready(&type);
  if (r < 0) throw PyError();

  PyObject* pyobj_type = reinterpret_cast<PyObject*>(&type);
  const char* name = std::strrchr(type.tp_name, '.');
  if (name) name++; else name = type.tp_name;

  r = PyModule_AddObject(module, name, pyobj_type);
  if (r < 0) throw PyError();
}


}  // namespace py

#endif
