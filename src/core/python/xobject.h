//------------------------------------------------------------------------------
// Copyright 2018-2021 H2O.ai
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the "Software"),
// to deal in the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense,
// and/or sell copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.
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
    size_t object_size;
    const char* class_name_;
    bool dynamic_type_;
    size_t : 56;
    std::vector<PyGetSetDef> get_defs;
    std::vector<PyMethodDef> meth_defs;

  public:
    static struct ConstructorTag {}   constructor_tag;
    static struct DestructorTag {}    destructor_tag;
    static struct GetSetTag {}        getset_tag;
    static struct MethodTag {}        method_tag;
    static struct Method0Tag {}       method0_tag;
    static struct ReprTag {}          repr_tag;
    static struct StrTag {}           str_tag;
    static struct LengthTag {}        length_tag;
    static struct GetattrTag {}       getattr_tag;
    static struct GetitemTag {}       getitem_tag;
    static struct SetitemTag {}       setitem_tag;
    static struct BuffersTag {}       buffers_tag;
    static struct IterTag {}          iter_tag;
    static struct NextTag {}          next_tag;
    static struct CallTag {}          call_tag;
    static struct HashTag {}          hash_tag;
    static struct RichCompareTag {}   rich_compare_tag;
    static struct NbAddTag {}         nb_add_tag;
    static struct NbSubtractTag {}    nb_subtract_tag;
    static struct NbMultiplyTag {}    nb_multiply_tag;
    static struct NbRemainderTag {}   nb_remainder_tag;
    static struct NbDivmodTag {}      nb_divmod_tag;
    static struct NbPowerTag {}       nb_power_tag;
    static struct NbNegativeTag {}    nb_negative_tag;
    static struct NbPositiveTag {}    nb_positive_tag;
    static struct NbAbsoluteTag {}    nb_absolute_tag;
    static struct NbInvertTag {}      nb_invert_tag;
    static struct NbBoolTag {}        nb_bool_tag;
    static struct NbLShiftTag {}      nb_lshift_tag;
    static struct NbRShiftTag {}      nb_rshift_tag;
    static struct NbAndTag {}         nb_and_tag;
    static struct NbXorTag {}         nb_xor_tag;
    static struct NbOrTag {}          nb_or_tag;
    static struct NbIntTag {}         nb_int_tag;
    static struct NbFloatTag {}       nb_float_tag;
    static struct NbFloorDivideTag {} nb_floordivide_tag;
    static struct NbTrueDivideTag {}  nb_truedivide_tag;

    XTypeMaker(size_t objsize, bool dynamic);
    void initialize_type();
    void attach_to_module(PyObject* module);
    void set_class_name(const char* name);
    void set_class_doc(const char* doc);
    void set_base_class(PyTypeObject* base_type);
    void set_subclassable(bool flag = true);

    void add_attr(const char* name, py::oobj value);

    // initproc = int(*)(PyObject*, PyObject*, PyObject*)
    void add(initproc _init, PKArgs& args, ConstructorTag);

    // destructor = void(*)(PyObject*)
    void add(destructor _dealloc, DestructorTag);



    // getter = PyObject*(*)(PyObject*, void*)
    // setter = int(*)(PyObject*, PyObject*, void*)
    void add(getter getfunc, setter setfunc, GSArgs& args, GetSetTag);

    // PyCFunctionWithKeywords = PyObject*(*)(PyObject*, PyObject*, PyObject*)
    void add(PyCFunctionWithKeywords meth, PKArgs& args, MethodTag);
    void add(XArgs* args, MethodTag);

    // unaryfunc = PyObject*(*)(PyObject*)
    void add(unaryfunc meth, const char* name, Method0Tag);

    // reprfunc = PyObject*(*)(PyObject*)
    void add(reprfunc _repr, ReprTag);

    // hashfunc = Py_hash_t(*)(PyObject*)
    void add(hashfunc _hash, HashTag);

    // reprfunc = PyObject*(*)(PyObject*)
    void add(reprfunc _str, StrTag);

    // lenfunc = Py_ssize_t(*)(PyObject*)
    void add(lenfunc getlen, LengthTag);

    // getattrofunc = PyObject*(*)(PyObject*, PyObject*)
    void add(getattrofunc getattr, GetattrTag);

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

    // richcmpfunc = PyObject*(*)(PyObject*, PyObject*, int)
    void add(richcmpfunc meth, RichCompareTag);

    // binaryfunc = PyObject*(*)(PyObject*, PyObject*)
    void add(binaryfunc meth, NbAddTag);
    void add(binaryfunc meth, NbSubtractTag);
    void add(binaryfunc meth, NbMultiplyTag);
    void add(binaryfunc meth, NbRemainderTag);
    void add(binaryfunc meth, NbDivmodTag);
    void add(ternaryfunc meth, NbPowerTag);
    void add(binaryfunc meth, NbLShiftTag);
    void add(binaryfunc meth, NbRShiftTag);
    void add(binaryfunc meth, NbAndTag);
    void add(binaryfunc meth, NbXorTag);
    void add(binaryfunc meth, NbOrTag);
    void add(binaryfunc meth, NbFloorDivideTag);
    void add(binaryfunc meth, NbTrueDivideTag);

    // unaryfunc = PyObject*(*)(PyObject*)
    void add(unaryfunc meth, NbNegativeTag);
    void add(unaryfunc meth, NbPositiveTag);
    void add(unaryfunc meth, NbAbsoluteTag);
    void add(unaryfunc meth, NbInvertTag);
    void add(unaryfunc meth, NbIntTag);
    void add(unaryfunc meth, NbFloatTag);

    // inquiry = int(*)(void*)
    void add(inquiry meth, NbBoolTag);

    void finalize();
    void finalize_getsets();
    void finalize_methods();

    PyObject* get_type_object() const;

  private:
    void init_tp_as_mapping();
    PyNumberMethods* tp_as_number();
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
template <typename Derived, bool DYNAMIC = false>
struct XObject : public PyObject {
  static PyObject* typePtr;
  size_t : 64 * DYNAMIC;  // __dict__
  size_t : 64 * DYNAMIC;  // __weakref__

  static void init_type(PyObject* module = nullptr) {
    static bool initialized = false;
    if (!initialized) {
      XTypeMaker xt(sizeof(Derived), DYNAMIC);
      Derived::impl_init_type(xt);
      xt.finalize();
      xt.attach_to_module(module);
      typePtr = xt.get_type_object();
      initialized = true;
    }
  }

  static bool check(PyObject* v) {
    if (!v) return false;
    int ret = PyObject_IsInstance(v, typePtr);
    if (ret == -1) PyErr_Clear();
    return (ret == 1);
  }

  static Derived* unchecked(PyObject* v) {
    return reinterpret_cast<Derived*>(v);
  }

  // Returns borrowed ref
  static Derived* cast_from(robj obj) {
    PyObject* v = obj.to_borrowed_ref();
    return check(v)? reinterpret_cast<Derived*>(v)
                   : nullptr;
  }

  template <typename... Args>
  static oobj make(Args... args) {
    return robj(typePtr).call({args...});
  }
};


template <typename Derived, bool DYNAMIC>
PyObject* XObject<Derived, DYNAMIC>::typePtr;




//------------------------------------------------------------------------------
// Exception-safe function implementations
//------------------------------------------------------------------------------

template <typename T, void(T::*METH)()>
void _safe_dealloc(PyObject* self) noexcept {
  auto cl = dt::CallLogger::dealloc(self);
  try {
    PyTypeObject* tp = Py_TYPE(self);
    (static_cast<T*>(self)->*METH)();
    tp->tp_free(self);
  }
  catch (const std::exception& e) {
    exception_to_python(e);
  }
}


// FIXME: this is used by METHOD0 only
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
    auto tself = static_cast<const T*>(self);
    return (tself->*METH)().release();
  }
  catch (const std::exception& e) {
    exception_to_python(e);
    return nullptr;
  }
}


template <class T, oobj(T::*METH)() const>
PyObject* _safe_getter(PyObject* obj, void* closure) noexcept {
  auto cl = dt::CallLogger::getset(obj, nullptr, closure);
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
  auto cl = dt::CallLogger::len(obj);
  try {
    T* t = static_cast<T*>(obj);
    return static_cast<Py_ssize_t>((t->*METH)());
  }
  catch (const std::exception& e) {
    exception_to_python(e);
    return -1;
  }
}


template <typename T, size_t(T::*METH)() const>
Py_hash_t _safe_hash(PyObject* obj) noexcept {
  auto cl = dt::CallLogger::hash(obj);
  try {
    T* t = static_cast<T*>(obj);
    return static_cast<Py_hash_t>((t->*METH)());
  }
  catch (const std::exception& e) {
    exception_to_python(e);
    return -1;
  }
}


template <typename T, py::oobj(T::*METH)(py::robj)>
PyObject* _safe_getattr(PyObject* self, PyObject* attr) noexcept {
  auto cl = dt::CallLogger::getattr(self, attr);
  try {
    T* tself = static_cast<T*>(self);
    return (tself->*METH)(py::robj(attr)).release();
  } catch (const std::exception& e) {
    exception_to_python(e);
    return nullptr;
  }
}


template <typename T, py::oobj(T::*METH)(py::robj)>
PyObject* _safe_getitem(PyObject* self, PyObject* key) noexcept {
  auto cl = dt::CallLogger::getsetitem(self, key, dt::CallLogger::GETITEM);
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


template <typename T, int(T::*METH)(Py_buffer*, int) noexcept>
int _safe_getbuffer(PyObject* self, Py_buffer* buf, int flags) noexcept {
  auto cl = dt::CallLogger::getbuffer(self, buf, flags);
  T* tself = static_cast<T*>(self);
  return (tself->*METH)(buf, flags);
}


template <typename T, void(T::*METH)(Py_buffer*) noexcept>
void _safe_releasebuffer(PyObject* self, Py_buffer* buf) noexcept {
  auto cl = dt::CallLogger::delbuffer(self, buf);
  T* tself = static_cast<T*>(self);
  (tself->*METH)(buf);
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
  auto cl = dt::CallLogger::getset(obj, value, closure);
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


template <typename T, py::oobj(T::*METH)(), int OP>
PyObject* _safe_unary(PyObject* self) noexcept {
  auto cl = dt::CallLogger::unaryfn(self, OP);
  try {
    auto tself = static_cast<T*>(self);
    return (tself->*METH)().release();
  } catch (const std::exception& e) {
    exception_to_python(e);
    return nullptr;
  }
}

template <py::oobj(*METH)(py::robj), int OP>
PyObject* _safe_uunary(PyObject* self) noexcept {
  auto cl = dt::CallLogger::unaryfn(self, OP);
  try {
    return METH(py::robj(self)).release();
  } catch (const std::exception& e) {
    exception_to_python(e);
    return nullptr;
  }
}


template <py::oobj(*METH)(py::robj, py::robj), int OP>
PyObject* _safe_binary(PyObject* self, PyObject* other) noexcept {
  auto cl = dt::CallLogger::binaryfn(self, other, OP);
  try {
    return METH(py::robj(self), py::robj(other)).release();
  } catch (const std::exception& e) {
    exception_to_python(e);
    return nullptr;
  }
}


template <py::oobj(*METH)(py::robj, py::robj, py::robj), int OP>
PyObject* _safe_ternary(PyObject* x, PyObject* y, PyObject* z) noexcept {
  auto cl = dt::CallLogger::ternaryfn(x, y, z, OP);
  try {
    return METH(py::robj(x), py::robj(y), py::robj(z)).release();
  } catch (const std::exception& e) {
    exception_to_python(e);
    return nullptr;
  }
}


template <typename T, bool(T::*METH)()>
int _safe_bool(PyObject* self) noexcept {
  auto cl = dt::CallLogger::unaryfn(self, dt::CallLogger::Op::__bool__);
  try {
    auto tself = static_cast<T*>(self);
    bool res = (tself->*METH)();
    return static_cast<int>(res);
  } catch (const std::exception& e) {
    exception_to_python(e);
    return -1;
  }
}


template <py::oobj(*METH)(py::robj, py::robj, int)>
PyObject* _safe_cmp(PyObject* x, PyObject* y, int op) noexcept {
  auto cl = dt::CallLogger::cmpfn(x, y, op);
  try {
    return METH(py::robj(x), py::robj(y), op).release();
  } catch (const std::exception& e) {
    exception_to_python(e);
    return nullptr;
  }
}




//------------------------------------------------------------------------------
// Helper macros
//------------------------------------------------------------------------------

#define CONSTRUCTOR(METH, ARGS)                                                \
    [](PyObject* self, PyObject* args, PyObject* kwds) noexcept -> int {       \
      return _call_method_int(METH, ARGS, self, args, kwds);                   \
    }, ARGS, py::XTypeMaker::constructor_tag


#define DESTRUCTOR(METH)                                                       \
    py::_safe_dealloc<CLASS_OF(METH), METH>, py::XTypeMaker::destructor_tag


#define GETTER(GETFN, ARGS)                                                    \
    py::_safe_getter<CLASS_OF(GETFN), GETFN>, nullptr,                         \
    ARGS, py::XTypeMaker::getset_tag


#define GETSET(GETFN, SETFN, ARGS)                                             \
    py::_safe_getter<CLASS_OF(GETFN), GETFN>,                                  \
    [](PyObject* self, PyObject* value, void* closure) noexcept -> int {       \
      return _call_setter(SETFN, ARGS._arg, self, value, closure);             \
    },                                                                         \
    ARGS, py::XTypeMaker::getset_tag


#define METHOD(METH, ARGS)                                                     \
    [](PyObject* self, PyObject* args, PyObject* kwds) noexcept -> PyObject* { \
      return _call_method(METH, ARGS, self, args, kwds);                       \
    }, ARGS, py::XTypeMaker::method_tag


// FIXME: this does not report function's name to the CallLogger
#define METHOD0(METH, NAME)                                                    \
    py::_safe_repr<CLASS_OF(METH), METH>, NAME, py::XTypeMaker::method0_tag


// Handles a request to the object to fill in `view` as specified by
// `flags`. If the object cannot provide a buffer of the exact type,
// it MUST raise PyExc_BufferError, set view->obj to NULL and
// return -1.
//
// On success, fill in `view`, set `view->obj` to a new reference to
// self and return 0.
//
// Note that both GETMETH and DELMETH must be noexcept, and if GETMETH
// needs to throw an exception it must do so via the standard python
// mechanism.
//
#define METHOD__GETBUFFER__(GETMETH, DELMETH)                                  \
    _safe_getbuffer<CLASS_OF(GETMETH), GETMETH>,                               \
    _safe_releasebuffer<CLASS_OF(DELMETH), DELMETH>,                           \
    py::XTypeMaker::buffers_tag


#define METHOD__REPR__(METH)                                                   \
    py::_safe_repr<CLASS_OF(METH), METH>, py::XTypeMaker::repr_tag


#define METHOD__STR__(METH)                                                    \
    py::_safe_repr<CLASS_OF(METH), METH>, py::XTypeMaker::str_tag


#define METHOD__LEN__(METH)                                                    \
    py::_safe_len<CLASS_OF(METH), METH>, py::XTypeMaker::length_tag


#define METHOD__HASH__(METH)                                                   \
    py::_safe_hash<CLASS_OF(METH), METH>, py::XTypeMaker::hash_tag


#define METHOD__GETATTR__(METH)                                                \
    py::_safe_getattr<CLASS_OF(METH), METH>, py::XTypeMaker::getattr_tag


#define METHOD__GETITEM__(METH)                                                \
    py::_safe_getitem<CLASS_OF(METH), METH>, py::XTypeMaker::getitem_tag


#define METHOD__SETITEM__(METH)                                                \
    py::_safe_setitem<CLASS_OF(METH), METH>, py::XTypeMaker::setitem_tag


#define METHOD__ITER__(METH)                                                   \
    py::_safe_unary<CLASS_OF(METH), METH, dt::CallLogger::Op::__iter__>,       \
    py::XTypeMaker::iter_tag


#define METHOD__REVERSED__(METH)                                               \
    METHOD0(METH, "__reversed__")


#define METHOD__NEXT__(METH)                                                   \
    py::_safe_unary<CLASS_OF(METH), METH, dt::CallLogger::Op::__next__>,       \
    py::XTypeMaker::next_tag


#define METHOD__LENGTH_HINT__(METH)                                            \
    METHOD0(METH, "__length_hint__")


#define METHOD__CALL__(METH, ARGS)                                             \
    [](PyObject* self, PyObject* args, PyObject* kwds) noexcept -> PyObject* { \
      return _call_method(METH, ARGS, self, args, kwds);                       \
    }, py::XTypeMaker::call_tag


#define METHOD__CMP__(METH)                                                    \
    py::_safe_cmp<METH>, py::XTypeMaker::rich_compare_tag


/**
  * Note: the arithmetic methods ought to be defined as
  *
  *           static oobj METH(robj lhs, robj rhs);
  *
  *       because python runtime may call this method with `this`
  *       being either `lhs` or `rhs`.
  */

#define METHOD__ADD__(METH)                                                    \
    py::_safe_binary<METH, dt::CallLogger::Op::__add__>,                       \
    py::XTypeMaker::nb_add_tag


#define METHOD__SUB__(METH)                                                    \
    py::_safe_binary<METH, dt::CallLogger::Op::__sub__>,                       \
    py::XTypeMaker::nb_subtract_tag


#define METHOD__MUL__(METH)                                                    \
    py::_safe_binary<METH, dt::CallLogger::Op::__mul__>,                       \
    py::XTypeMaker::nb_multiply_tag


#define METHOD__MOD__(METH)                                                    \
    py::_safe_binary<METH, dt::CallLogger::Op::__mod__>,                       \
    py::XTypeMaker::nb_remainder_tag


#define METHOD__DIVMOD__(METH)                                                 \
    py::_safe_binary<METH, dt::CallLogger::Op::__divmod__>,                    \
    py::XTypeMaker::nb_divmod_tag


#define METHOD__POW__(METH)                                                    \
    _safe_ternary<METH, dt::CallLogger::Op::__pow__>,                          \
    py::XTypeMaker::nb_power_tag


#define METHOD__LSHIFT__(METH)                                                 \
    py::_safe_binary<METH, dt::CallLogger::Op::__lshift__>,                    \
    py::XTypeMaker::nb_lshift_tag


#define METHOD__RSHIFT__(METH)                                                 \
    py::_safe_binary<METH, dt::CallLogger::Op::__rshift__>,                    \
    py::XTypeMaker::nb_rshift_tag


#define METHOD__AND__(METH)                                                    \
    py::_safe_binary<METH, dt::CallLogger::Op::__and__>,                       \
    py::XTypeMaker::nb_and_tag


#define METHOD__XOR__(METH)                                                    \
    py::_safe_binary<METH, dt::CallLogger::Op::__xor__>,                       \
    py::XTypeMaker::nb_xor_tag


#define METHOD__OR__(METH)                                                     \
    py::_safe_binary<METH, dt::CallLogger::Op::__or__>,                        \
    py::XTypeMaker::nb_or_tag


#define METHOD__FLOORDIV__(METH)                                               \
    py::_safe_binary<METH, dt::CallLogger::Op::__floordiv__>,                  \
    py::XTypeMaker::nb_floordivide_tag


#define METHOD__TRUEDIV__(METH)                                                \
    py::_safe_binary<METH, dt::CallLogger::Op::__truediv__>,                   \
    py::XTypeMaker::nb_truedivide_tag


/**
  * Unary arithmetic operators
  */

#define METHOD__NEG__(METH)                                                    \
    py::_safe_uunary<METH, dt::CallLogger::Op::__neg__>,                       \
    py::XTypeMaker::nb_negative_tag


#define METHOD__POS__(METH)                                                    \
    py::_safe_uunary<METH, dt::CallLogger::Op::__pos__>,                        \
    py::XTypeMaker::nb_positive_tag


#define METHOD__ABS__(METH)                                                    \
    py::_safe_unary<CLASS_OF(METH), METH, dt::CallLogger::Op::__abs__>,        \
    py::XTypeMaker::nb_absolute_tag


#define METHOD__INVERT__(METH)                                                 \
    py::_safe_uunary<METH, dt::CallLogger::Op::__invert__>,                    \
    py::XTypeMaker::nb_invert_tag


#define METHOD__BOOL__(METH)                                                   \
    py::_safe_bool<CLASS_OF(METH), METH>, py::XTypeMaker::nb_bool_tag


#define METHOD__INT__(METH)                                                    \
    py::_safe_unary<CLASS_OF(METH), METH, dt::CallLogger::Op::__int__>,        \
    py::XTypeMaker::nb_int_tag


#define METHOD__FLOAT__(METH)                                                  \
    py::_safe_unary<CLASS_OF(METH), METH, dt::CallLogger::Op::__float__>,      \
    py::XTypeMaker::nb_float_tag




} // namespace py
#endif
