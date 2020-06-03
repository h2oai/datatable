//------------------------------------------------------------------------------
// Copyright 2018-2020 H2O.ai
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
#include "call_logger.h"
#include "python/args.h"
#include "python/obj.h"
#include "utils/assert.h"
#include "utils/macros.h"
namespace py {



//------------------------------------------------------------------------------
// XTypeMaker
//------------------------------------------------------------------------------

/**
 * Helper class for initializing the `PyTypeObject type` variable.
 *
 * See also: https://docs.python.org/3/c-api/typeobj.html
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
    static struct Method0Tag {} method0_tag;
    static struct ReprTag {} repr_tag;
    static struct StrTag {} str_tag;
    static struct LengthTag {} length_tag;
    static struct GetitemTag {} getitem_tag;
    static struct SetitemTag {} setitem_tag;
    static struct BuffersTag {} buffers_tag;
    static struct IterTag {} iter_tag;
    static struct NextTag {} next_tag;
    static struct CallTag {} call_tag;

    XTypeMaker(PyTypeObject* t, size_t objsize);
    void attach_to_module(PyObject* module);
    void set_class_name(const char* name);
    void set_class_doc(const char* doc);
    void set_base_class(PyTypeObject* base_type);
    void set_subclassable(bool flag = true);

    // initproc = int(*)(PyObject*, PyObject*, PyObject*)
    void add(initproc _init, PKArgs& args, ConstructorTag);

    // destructor = void(*)(PyObject*)
    void add(destructor _dealloc, DestructorTag);



    // getter = PyObject*(*)(PyObject*, void*)
    // setter = int(*)(PyObject*, PyObject*, void*)
    void add(getter getfunc, setter setfunc, GSArgs& args, GetSetTag);

    // PyCFunctionWithKeywords = PyObject*(*)(PyObject*, PyObject*, PyObject*)
    void add(PyCFunctionWithKeywords meth, PKArgs& args, MethodTag);

    // unaryfunc = PyObject*(*)(PyObject*)
    void add(unaryfunc meth, const char* name, Method0Tag);

    // reprfunc = PyObject*(*)(PyObject*)
    void add(reprfunc _repr, ReprTag);

    // reprfunc = PyObject*(*)(PyObject*)
    void add(reprfunc _str, StrTag);

    // lenfunc = Py_ssize_t(*)(PyObject*)
    void add(lenfunc getlen, LengthTag);

    // binaryfunc = PyObject*(*)(PyObject*, PyObject*)
    void add(binaryfunc _getitem, GetitemTag);

    // objobjargproc = int(*)(PyObject*, PyObject*, PyObject*)
    void add(objobjargproc _setitem, SetitemTag);

    // getbufferproc = int(*)(PyObject*, Py_buffer*, int)
    // releasebufferproc = void(*)(PyObject*, Py_buffer*)
    void add(getbufferproc _get, releasebufferproc _del, BuffersTag);

    // getiterfunc = PyObject*(*)(PyObject*)
    void add(getiterfunc _iter, IterTag);

    // iternextfunc = PyObject*(*)(PyObject*)
    void add(iternextfunc _next, NextTag);

    // ternaryfunc = PyObject*(*)(PyObject*, PyObject*, PyObject*)
    void add(ternaryfunc meth, CallTag);

  private:
    PyGetSetDef* finalize_getsets();
    PyMethodDef* finalize_methods();
    void init_tp_as_mapping();

};




//------------------------------------------------------------------------------
// XObject
//------------------------------------------------------------------------------

/**
 * This functionality can be used in order to create new Python classes
 * from C++. In Python documentation they are called "extension types".
 *
 * This framework is the syntactic sugar over the "native" Python C API, as
 * described e.g. here: https://docs.python.org/3/extending/newtypes.html
 * You do not need to know the native API in order to use this framework.
 *
 * Here's a minimal example of how to declare a new python class:
 *
 *    class MyObject : public XObject<MyObject> {
 *      ...
 *      public:
 *        static void impl_init_type(XTypeMaker& xt) {
 *          xt.set_class_name("datatable.MyObject");
 *          static PKArgs args_init(0,0,0,false,false,{},"__init__",nullptr);
 *          xt.add(CONSTRUCTOR(&MyObject::m__init__, args_init));
 *          xt.add(DESTRUCTOR(&MyObject::m__dealloc__));
 *        }
 *        void m__init__(const py::PKArgs&);
 *        void m__dealloc__();
 *    };
 *
 * Then, in order to attach this class to a python module, call at the module
 * initialization stage:
 *
 *    MyObject::init_type(module);
 *
 * If an error occurs during initialization, an exception will be thrown.
 *
 * There are multiple class properties that can be set up within the
 * `impl_init_type()`, check `XTypeMaker` for more info. The three properties
 * shown in the example above are required, all other are optional.
 *
 */
template <typename Derived>
struct XObject : public PyObject {
  static PyTypeObject type;

  static void init_type(PyObject* module = nullptr) {
    static bool initalized = false;
    if (initalized) return;
    XTypeMaker xt(&type, sizeof(Derived));
    Derived::impl_init_type(xt);
    xt.attach_to_module(module);
    initalized = true;
  }

  static bool check(PyObject* v) {
    if (!v) return false;
    auto typeptr = reinterpret_cast<PyObject*>(&type);
    int ret = PyObject_IsInstance(v, typeptr);
    if (ret == -1) PyErr_Clear();
    return (ret == 1);
  }

  // Returns borrowed ref
  static Derived* cast_from(robj obj) {
    PyObject* v = obj.to_borrowed_ref();
    return check(v)? reinterpret_cast<Derived*>(v)
                   : nullptr;
  }

  template <typename... Args>
  static oobj make(Args... args) {
    robj rtype(reinterpret_cast<PyObject*>(&type));
    return rtype.call({args...});
  }
};


template <typename D>
PyTypeObject XObject<D>::type;




//------------------------------------------------------------------------------
// Exception-safe function implementations
//------------------------------------------------------------------------------

template <typename T, void(T::*METH)()>
void _safe_dealloc(PyObject* self) noexcept {
  auto cl = dt::CallLogger::dealloc(self);
  try {
    (static_cast<T*>(self)->*METH)();
  }
  catch (const std::exception& e) {
    exception_to_python(e);
  }
}


template <typename T, py::oobj(T::*METH)()>
PyObject* _safe_repr(PyObject* self) noexcept {
  // Do not use CallLogger here, because it itself calls repr() on
  // the arguments passed
  try {
    T* tself = static_cast<T*>(self);
    return (tself->*METH)().release();
  }
  catch (const std::exception& e) {
    exception_to_python(e);
    return nullptr;
  }
}


template <typename T, py::oobj(T::*METH)() const>
PyObject* _safe_repr(PyObject* self) noexcept {
  // Do not use CallLogger here, because it itself calls repr() on
  // the arguments passed
  try {
    T* tself = static_cast<T*>(self);
    return (tself->*METH)().release();
  }
  catch (const std::exception& e) {
    exception_to_python(e);
    return nullptr;
  }
}


template <class T, oobj(T::*METH)() const>
PyObject* _safe_getter(PyObject* obj, void* closure) noexcept {
  auto cl = dt::CallLogger::getsetattr(obj, nullptr, closure);
  try {
    T* t = static_cast<T*>(obj);
    return (t->*METH)().release();
  }
  catch (const std::exception& e) {
    exception_to_python(e);
    return nullptr;
  }
}


template <typename T, size_t(T::*METH)() const>
Py_ssize_t _safe_len(PyObject* obj) noexcept {
  try {
    T* t = static_cast<T*>(obj);
    return static_cast<Py_ssize_t>((t->*METH)());
  }
  catch (const std::exception& e) {
    exception_to_python(e);
    return -1;
  }
}


template <typename T, py::oobj(T::*METH)(py::robj)>
PyObject* _safe_getitem(PyObject* self, PyObject* key) noexcept {
  auto cl = dt::CallLogger::getsetitem(self, key, nullptr);
  try {
    T* tself = static_cast<T*>(self);
    return (tself->*METH)(py::robj(key)).release();
  } catch (const std::exception& e) {
    exception_to_python(e);
    return nullptr;
  }
}


template <typename T, void(T::*METH)(py::robj, py::robj)>
int _safe_setitem(PyObject* self, PyObject* key, PyObject* val) noexcept {
  auto cl = dt::CallLogger::getsetitem(self, key, val);
  try {
    T* tself = static_cast<T*>(self);
    (tself->*METH)(py::robj(key), py::robj(val));
    return 0;
  }
  catch (const std::exception& e) {
    exception_to_python(e);
    return -1;
  }
}


template <typename T, void(T::*METH)(Py_buffer*, int)>
int _safe_getbuffer(PyObject* self, Py_buffer* buf, int flags) noexcept {
  try {
    T* tself = static_cast<T*>(self);
    (tself->*METH)(buf, flags);
    return 0;
  }
  catch (const std::exception& e) {
    exception_to_python(e);
    return -1;
  }
}


template <typename T, void(T::*METH)(Py_buffer*)>
void _safe_releasebuffer(PyObject* self, Py_buffer* buf) noexcept {
  try {
    T* tself = static_cast<T*>(self);
    (tself->*METH)(buf);
  }
  catch (const std::exception& e) {
    exception_to_python(e);
  }
}


template <class T>
PyObject* _call_method(
    oobj(T::*fn)(const PKArgs&), PKArgs& ARGS,
    PyObject* obj, PyObject* args, PyObject* kwds) noexcept
{
  auto cl = dt::CallLogger::method(&ARGS, obj, args, kwds);
  try {
    ARGS.bind(args, kwds);
    T* tobj = reinterpret_cast<T*>(obj);
    return (tobj->*fn)(ARGS).release();
  }
  catch (const std::exception& e) {
    exception_to_python(e);
    return nullptr;
  }
}


template <class T>
PyObject* _call_method(
    void(T::*fn)(const PKArgs&), PKArgs& ARGS,
    PyObject* obj, PyObject* args, PyObject* kwds) noexcept
{
  auto cl = dt::CallLogger::method(&ARGS, obj, args, kwds);
  try {
    ARGS.bind(args, kwds);
    T* tobj = static_cast<T*>(obj);
    (tobj->*fn)(ARGS);
    return py::None().release();
  }
  catch (const std::exception& e) {
    exception_to_python(e);
    return nullptr;
  }
}


template <class T>
int _call_method_int(
    void(T::*fn)(const PKArgs&), PKArgs& ARGS,
    PyObject* obj, PyObject* args, PyObject* kwds) noexcept
{
  auto cl = dt::CallLogger::method(&ARGS, obj, args, kwds);
  try {
    ARGS.bind(args, kwds);
    T* tobj = static_cast<T*>(obj);
    (tobj->*fn)(ARGS);
    return 0;
  }
  catch (const std::exception& e) {
    exception_to_python(e);
    return -1;
  }
}


template <class T>
int _call_setter(void(T::*fn)(const Arg&), Arg& ARG,
                 PyObject* obj, PyObject* value, void* closure) noexcept
{
  auto cl = dt::CallLogger::getsetattr(obj, value, closure);
  try {
    ARG.set(value);
    T* tobj = static_cast<T*>(obj);
    (tobj->*fn)(ARG);
    return 0;
  }
  catch (const std::exception& e) {
    exception_to_python(e);
    return -1;
  }
}



//------------------------------------------------------------------------------
// Helper macros
//------------------------------------------------------------------------------

#if DT_COMPILER_CLANG
  #pragma clang diagnostic push
  #pragma clang diagnostic ignored "-Wunused-template"
#endif

template <typename T, typename R, typename... Args>
static T _class_of_impl(R(T::*)(Args...));

template <typename T, typename R, typename... Args>
static T _class_of_impl(R(T::*)(Args...) const);

#define CLASS_OF(METH) decltype(_class_of_impl(METH))

#if DT_COMPILER_CLANG
  #pragma clang diagnostic pop
#endif


#define CONSTRUCTOR(METH, ARGS)                                                \
    [](PyObject* self, PyObject* args, PyObject* kwds) noexcept -> int {       \
      return _call_method_int(METH, ARGS, self, args, kwds);                   \
    }, ARGS, py::XTypeMaker::constructor_tag


#define DESTRUCTOR(METH)                                                       \
    py::_safe_dealloc<CLASS_OF(METH), METH>, py::XTypeMaker::destructor_tag


#define GETTER(GETFN, ARGS)                                                    \
    _safe_getter<CLASS_OF(GETFN), GETFN>, nullptr,                             \
    ARGS, py::XTypeMaker::getset_tag


#define GETSET(GETFN, SETFN, ARGS)                                             \
    _safe_getter<CLASS_OF(GETFN), GETFN>,                                      \
    [](PyObject* self, PyObject* value, void* closure) noexcept -> int {       \
      return _call_setter(SETFN, ARGS._arg, self, value, closure);             \
    },                                                                         \
    ARGS, py::XTypeMaker::getset_tag


#define METHOD(METH, ARGS)                                                     \
    [](PyObject* self, PyObject* args, PyObject* kwds) noexcept -> PyObject* { \
      return _call_method(METH, ARGS, self, args, kwds);                       \
    }, ARGS, py::XTypeMaker::method_tag


#define METHOD0(METH, NAME)                                                    \
    _safe_repr<CLASS_OF(METH), METH>, NAME, py::XTypeMaker::method0_tag


#define BUFFERS(GETMETH, DELMETH)                                              \
    _safe_getbuffer<CLASS_OF(GETMETH), GETMETH>,                               \
    _safe_releasebuffer<CLASS_OF(DELMETH), DELMETH>,                           \
    py::XTypeMaker::buffers_tag


#define METHOD__REPR__(METH)                                                   \
    _safe_repr<CLASS_OF(METH), METH>, py::XTypeMaker::repr_tag


#define METHOD__STR__(METH)                                                    \
    _safe_repr<CLASS_OF(METH), METH>, py::XTypeMaker::str_tag


#define METHOD__LEN__(METH)                                                    \
    _safe_len<CLASS_OF(METH), METH>, py::XTypeMaker::length_tag


#define METHOD__GETITEM__(METH)                                                \
    _safe_getitem<CLASS_OF(METH), METH>, py::XTypeMaker::getitem_tag


#define METHOD__SETITEM__(METH)                                                \
    _safe_setitem<CLASS_OF(METH), METH>, py::XTypeMaker::setitem_tag


#define METHOD__ITER__(METH)                                                   \
    _safe_repr<CLASS_OF(METH), METH>, py::XTypeMaker::iter_tag


#define METHOD__REVERSED__(METH)                                               \
    METHOD0(METH, "__reversed__")


#define METHOD__NEXT__(METH)                                                   \
    _safe_repr<CLASS_OF(METH), METH>, py::XTypeMaker::next_tag


#define METHOD__LENGTH_HINT__(METH)                                            \
    METHOD0(METH, "__length_hint__")


#define METHOD__CALL__(METH, ARGS)                                             \
    [](PyObject* self, PyObject* args, PyObject* kwds) noexcept -> PyObject* { \
      return _call_method(METH, ARGS, self, args, kwds);                       \
    }, py::XTypeMaker::call_tag


} // namespace py
#endif
