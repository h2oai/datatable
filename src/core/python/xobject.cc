//------------------------------------------------------------------------------
// Copyright 2019-2021 H2O.ai
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
#include "python/xobject.h"
#include "python/xargs.h"
namespace py {


XTypeMaker::ConstructorTag   XTypeMaker::constructor_tag;
XTypeMaker::DestructorTag    XTypeMaker::destructor_tag;
XTypeMaker::GetSetTag        XTypeMaker::getset_tag;
XTypeMaker::MethodTag        XTypeMaker::method_tag;
XTypeMaker::Method0Tag       XTypeMaker::method0_tag;
XTypeMaker::ReprTag          XTypeMaker::repr_tag;
XTypeMaker::StrTag           XTypeMaker::str_tag;
XTypeMaker::LengthTag        XTypeMaker::length_tag;
XTypeMaker::GetattrTag       XTypeMaker::getattr_tag;
XTypeMaker::GetitemTag       XTypeMaker::getitem_tag;
XTypeMaker::SetitemTag       XTypeMaker::setitem_tag;
XTypeMaker::BuffersTag       XTypeMaker::buffers_tag;
XTypeMaker::IterTag          XTypeMaker::iter_tag;
XTypeMaker::NextTag          XTypeMaker::next_tag;
XTypeMaker::CallTag          XTypeMaker::call_tag;
XTypeMaker::HashTag          XTypeMaker::hash_tag;
XTypeMaker::RichCompareTag   XTypeMaker::rich_compare_tag;
XTypeMaker::NbAddTag         XTypeMaker::nb_add_tag;
XTypeMaker::NbSubtractTag    XTypeMaker::nb_subtract_tag;
XTypeMaker::NbMultiplyTag    XTypeMaker::nb_multiply_tag;
XTypeMaker::NbRemainderTag   XTypeMaker::nb_remainder_tag;
XTypeMaker::NbDivmodTag      XTypeMaker::nb_divmod_tag;
XTypeMaker::NbPowerTag       XTypeMaker::nb_power_tag;
XTypeMaker::NbNegativeTag    XTypeMaker::nb_negative_tag;
XTypeMaker::NbPositiveTag    XTypeMaker::nb_positive_tag;
XTypeMaker::NbAbsoluteTag    XTypeMaker::nb_absolute_tag;
XTypeMaker::NbInvertTag      XTypeMaker::nb_invert_tag;
XTypeMaker::NbBoolTag        XTypeMaker::nb_bool_tag;
XTypeMaker::NbLShiftTag      XTypeMaker::nb_lshift_tag;
XTypeMaker::NbRShiftTag      XTypeMaker::nb_rshift_tag;
XTypeMaker::NbAndTag         XTypeMaker::nb_and_tag;
XTypeMaker::NbXorTag         XTypeMaker::nb_xor_tag;
XTypeMaker::NbOrTag          XTypeMaker::nb_or_tag;
XTypeMaker::NbIntTag         XTypeMaker::nb_int_tag;
XTypeMaker::NbFloatTag       XTypeMaker::nb_float_tag;
XTypeMaker::NbFloorDivideTag XTypeMaker::nb_floordivide_tag;
XTypeMaker::NbTrueDivideTag  XTypeMaker::nb_truedivide_tag;


XTypeMaker::XTypeMaker(size_t objsize, bool dynamic)
  : type(nullptr),
    object_size(objsize),
    dynamic_type_(dynamic)
{}


// The type can only be initialized after its name is known
void XTypeMaker::initialize_type() {
  xassert(type == nullptr);
  xassert(class_name_ != nullptr);
  if (dynamic_type_) {  // dynamically created python type
    py::odict defs;
    defs.set(py::ostring("__module__"), py::ostring("datatable"));
    auto typeObj = py::oobj(&PyType_Type).call({
        py::ostring(class_name_), py::otuple(0), defs
    });
    type = reinterpret_cast<PyTypeObject*>(std::move(typeObj).release());
    xassert(type->tp_basicsize == sizeof(PyObject) + 16);
    type->tp_basicsize = static_cast<Py_ssize_t>(object_size);
  }
  else {
    type = new PyTypeObject;
    std::memset(type, 0, sizeof(PyTypeObject));
    Py_INCREF(type);
    type->tp_basicsize = static_cast<Py_ssize_t>(object_size);
    type->tp_itemsize = 0;
    type->tp_flags = Py_TPFLAGS_DEFAULT;
    type->tp_alloc = &PyType_GenericAlloc;
    type->tp_new   = &PyType_GenericNew;
    type->tp_name  = class_name_;
  }
}


void XTypeMaker::finalize() {
  xassert(type->tp_dealloc);
  xassert(type->tp_init);
  xassert(type->tp_name);
  finalize_getsets();
  finalize_methods();
  if (!dynamic_type_) {
    int r = PyType_Ready(type);
    if (r < 0) throw PyError();
  }
}


PyObject* XTypeMaker::get_type_object() const {
  return reinterpret_cast<PyObject*>(type);
}


void XTypeMaker::attach_to_module(PyObject* module) {
  if (!module) return;
  const char* dot = std::strrchr(type->tp_name, '.');
  const char* name = dot? dot + 1 : type->tp_name;
  int r = PyModule_AddObject(module, name, reinterpret_cast<PyObject*>(type));
  if (r < 0) throw PyError();
}


void XTypeMaker::set_class_name(const char* name) {
  class_name_ = name;
  initialize_type();
}


void XTypeMaker::set_class_doc(const char* doc) {
  xassert(type);
  if (dynamic_type_) {
    // For dynamic type, the documentation must be stored both in `tp_doc`
    // (as a copy of the original string), and as a `__doc__` attribute in
    // the class' __dict__.
    size_t len = std::strlen(doc);
    char* doc_copy = static_cast<char*>(PyObject_MALLOC(len + 1));
    std::memcpy(doc_copy, doc, len + 1);
    type->tp_doc = doc_copy;
    py::rdict::unchecked(type->tp_dict)
        .set(py::ostring("__doc__"), py::ostring(doc));
  }
  else {
    type->tp_doc = doc;
  }
}


void XTypeMaker::set_base_class(PyTypeObject* base_type) {
  xassert(type);
  type->tp_base = base_type;
}


void XTypeMaker::set_subclassable(bool flag /* = true */) {
  xassert(type);
  if (flag) {
    type->tp_flags |= Py_TPFLAGS_BASETYPE;
  } else {
    type->tp_flags &= ~Py_TPFLAGS_BASETYPE;
  }
}


// An attribute can only be set on a dynamically allocated type
void XTypeMaker::add_attr(const char* name, py::oobj value) {
  xassert(type);
  xassert(dynamic_type_);
  py::robj(type).set_attr(name, value);
}


// initproc = int(*)(PyObject*, PyObject*, PyObject*)
void XTypeMaker::add(initproc _init, PKArgs& args, ConstructorTag) {
  xassert(type);
  args.set_class_name(type->tp_name);
  type->tp_init = _init;
}


// destructor = void(*)(PyObject*)
void XTypeMaker::add(destructor _dealloc, DestructorTag) {
  xassert(type);
  type->tp_dealloc = _dealloc;
}


// getter = PyObject*(*)(PyObject*, void*)
// setter = int(*)(PyObject*, PyObject*, void*)
void XTypeMaker::add(getter getfunc, setter setfunc, GSArgs& args, GetSetTag) {
  xassert(type);
  args.class_name = type->tp_name;
  get_defs.push_back(PyGetSetDef {
    const_cast<char*>(args.name),
    getfunc, setfunc,
    const_cast<char*>(args.doc),
    &args  // closure
  });
}


// PyCFunctionWithKeywords = PyObject*(*)(PyObject*, PyObject*, PyObject*)
void XTypeMaker::add(PyCFunctionWithKeywords meth, PKArgs& args, MethodTag) {
  xassert(type);
  args.set_class_name(type->tp_name);
  meth_defs.push_back(PyMethodDef {
    args.get_short_name(),
    reinterpret_cast<PyCFunction>(meth),
    METH_VARARGS | METH_KEYWORDS,
    args.get_docstring()
  });
}

void XTypeMaker::add(XArgs* args, MethodTag) {
  xassert(type);
  args->set_class_name(type->tp_name);
  meth_defs.push_back(args->get_method_def());
}


// unaryfunc = PyObject*(*)(PyObject*)
void XTypeMaker::add(unaryfunc meth, const char* name, Method0Tag) {
  xassert(type);
  meth_defs.push_back(PyMethodDef {
    name, reinterpret_cast<PyCFunction>(meth),
    METH_NOARGS, nullptr
  });
}


// reprfunc = PyObject*(*)(PyObject*)
void XTypeMaker::add(reprfunc _repr, ReprTag) {
  xassert(type);
  type->tp_repr = _repr;
}


// reprfunc = PyObject*(*)(PyObject*)
void XTypeMaker::add(reprfunc _str, StrTag) {
  xassert(type);
  type->tp_str = _str;
}


// lenfunc = Py_ssize_t(*)(PyObject*)
void XTypeMaker::add(lenfunc _length, LengthTag) {
  xassert(type);
  init_tp_as_mapping();
  type->tp_as_mapping->mp_length = _length;
}

// hashfunc - Py_hash_t(*)(PyObject*)
void XTypeMaker::add(hashfunc _hash, HashTag) {
  xassert(type);
  type->tp_hash = _hash;
}

// getattrofunc = PyObject*(*)(PyObject*, PyObject*)
void XTypeMaker::add(getattrofunc getattr, GetattrTag) {
  xassert(type);
  type->tp_getattro = getattr;
}


// binaryfunc = PyObject*(*)(PyObject*, PyObject*)
void XTypeMaker::add(binaryfunc _getitem, GetitemTag) {
  xassert(type);
  init_tp_as_mapping();
  type->tp_as_mapping->mp_subscript = _getitem;
}


// objobjargproc = int(*)(PyObject*, PyObject*, PyObject*)
void XTypeMaker::add(objobjargproc _setitem, SetitemTag) {
  xassert(type);
  init_tp_as_mapping();
  type->tp_as_mapping->mp_ass_subscript = _setitem;
}


// getbufferproc = int(*)(PyObject*, Py_buffer*, int)
// releasebufferproc = void(*)(PyObject*, Py_buffer*)
void XTypeMaker::add(getbufferproc _get, releasebufferproc _del, BuffersTag) {
  xassert(type);
  xassert(type->tp_as_buffer == nullptr);
  PyBufferProcs* bufs = new PyBufferProcs();
  bufs->bf_getbuffer = _get;
  bufs->bf_releasebuffer = _del;
  type->tp_as_buffer = bufs;
}


// getiterfunc = PyObject*(*)(PyObject*)
void XTypeMaker::add(getiterfunc _iter, IterTag) {
  xassert(type);
  type->tp_iter = _iter;
}


// iternextfunc = PyObject*(*)(PyObject*)
void XTypeMaker::add(iternextfunc _next, NextTag) {
  xassert(type);
  if (!type->tp_iter) {
    type->tp_iter = PyObject_SelfIter;
  }
  type->tp_iternext = _next;
}


// ternaryfunc = PyObject*(*)(PyObject*, PyObject*, PyObject*)
void XTypeMaker::add(ternaryfunc meth, CallTag) {
  xassert(type);
  type->tp_call = meth;
}


// richcmpfunc = PyObject*(*)(PyObject*, PyObject*, int)
void XTypeMaker::add(richcmpfunc meth, RichCompareTag) {
  xassert(type);
  type->tp_richcompare = meth;
}


// binaryfunc = PyObject*(*)(PyObject*, PyObject*)
void XTypeMaker::add(binaryfunc fn, NbAddTag)         { tp_as_number()->nb_add = fn; }
void XTypeMaker::add(binaryfunc fn, NbSubtractTag)    { tp_as_number()->nb_subtract = fn; }
void XTypeMaker::add(binaryfunc fn, NbMultiplyTag)    { tp_as_number()->nb_multiply = fn; }
void XTypeMaker::add(binaryfunc fn, NbRemainderTag)   { tp_as_number()->nb_remainder = fn; }
void XTypeMaker::add(binaryfunc fn, NbDivmodTag)      { tp_as_number()->nb_divmod = fn; }
void XTypeMaker::add(binaryfunc fn, NbLShiftTag)      { tp_as_number()->nb_lshift = fn; }
void XTypeMaker::add(binaryfunc fn, NbRShiftTag)      { tp_as_number()->nb_rshift = fn; }
void XTypeMaker::add(binaryfunc fn, NbAndTag)         { tp_as_number()->nb_and = fn; }
void XTypeMaker::add(binaryfunc fn, NbXorTag)         { tp_as_number()->nb_xor = fn; }
void XTypeMaker::add(binaryfunc fn, NbOrTag)          { tp_as_number()->nb_or = fn; }
void XTypeMaker::add(binaryfunc fn, NbTrueDivideTag)  { tp_as_number()->nb_true_divide = fn; }
void XTypeMaker::add(binaryfunc fn, NbFloorDivideTag) { tp_as_number()->nb_floor_divide = fn; }

void XTypeMaker::add(unaryfunc fn, NbNegativeTag)     { tp_as_number()->nb_negative = fn; }
void XTypeMaker::add(unaryfunc fn, NbPositiveTag)     { tp_as_number()->nb_positive = fn; }
void XTypeMaker::add(unaryfunc fn, NbAbsoluteTag)     { tp_as_number()->nb_absolute = fn; }
void XTypeMaker::add(unaryfunc fn, NbInvertTag)       { tp_as_number()->nb_invert = fn; }
void XTypeMaker::add(unaryfunc fn, NbIntTag)          { tp_as_number()->nb_int = fn; }
void XTypeMaker::add(unaryfunc fn, NbFloatTag)        { tp_as_number()->nb_float = fn; }
void XTypeMaker::add(inquiry fn, NbBoolTag)           { tp_as_number()->nb_bool = fn; }
void XTypeMaker::add(ternaryfunc fn, NbPowerTag)      { tp_as_number()->nb_power = fn; }


void XTypeMaker::finalize_getsets() {
  size_t n = get_defs.size();
  size_t n0 = 0;
  if (!n) return;
  if (type->tp_getset) {
    while ((type->tp_getset)[n0].name != nullptr) n0++;
  }
  PyGetSetDef* res = new PyGetSetDef[n + n0 + 1];
  if (n0) {
    std::memcpy(res, type->tp_getset, n0 * sizeof(PyGetSetDef));
    // type->tp_getset is allocated statically in typeobject.c, so
    // it doesn't need to be freed. It also means that the pointer `res` will
    // never be freed either (but that's ok since we want our types to live
    // forever anyways).
  }
  std::memcpy(res + n0, get_defs.data(), n * sizeof(PyGetSetDef));
  std::memset(res + n0 + n, 0, sizeof(PyGetSetDef));
  type->tp_getset = res;

  if (dynamic_type_) {
    // See CPython:typeobject.cc:add_getset()
    for (size_t i = n0; i < n + n0; i++) {
      PyObject* descr = PyDescr_NewGetSet(type, type->tp_getset + i);
      if (!descr) throw PyError();
      py::rdict(type->tp_dict).set(
          py::robj(PyDescr_NAME(descr)),
          py::oobj::from_new_reference(descr)
      );
    }
  }
}


void XTypeMaker::finalize_methods() {
  size_t n = meth_defs.size();
  size_t n0 = 0;
  if (!n) return;
  if (type->tp_methods) {
    while ((type->tp_methods)[n0].ml_name != nullptr) n0++;
  }
  PyMethodDef* res = new PyMethodDef[n + n0 + 1];
  if (n0) {
    std::memcpy(res, type->tp_methods, n0 * sizeof(PyMethodDef));
  }
  std::memcpy(res + n0, meth_defs.data(), n * sizeof(PyMethodDef));
  std::memset(res + n0 + n, 0, sizeof(PyMethodDef));
  type->tp_methods = res;

  if (dynamic_type_) {
    // See CPython:typeobject.cc:add_methods()
    PyObject* dict = type->tp_dict;
    for (size_t i = n0; i < n + n0; i++) {
      PyMethodDef* meth = res + i;
      if (meth->ml_flags & METH_CLASS) {
        throw NotImplError() << "Class methods not supported";
      }
      if (meth->ml_flags & METH_STATIC) {
        PyObject* cfunc = PyCFunction_NewEx(meth, reinterpret_cast<PyObject*>(type), nullptr);
        PyObject* descr = PyStaticMethod_New(cfunc);
        Py_DECREF(cfunc);
        PyDict_SetItemString(dict, meth->ml_name, descr);
        Py_DECREF(descr);
      } else {
        PyObject* descr = PyDescr_NewMethod(type, meth);
        PyDict_SetItem(dict, PyDescr_NAME(descr), descr);
        Py_DECREF(descr);
      }
    }
  }
}


void XTypeMaker::init_tp_as_mapping() {
  xassert(type);
  if (type->tp_as_mapping) return;
  type->tp_as_mapping = new PyMappingMethods;
  type->tp_as_mapping->mp_length = nullptr;
  type->tp_as_mapping->mp_subscript = nullptr;
  type->tp_as_mapping->mp_ass_subscript = nullptr;
}


PyNumberMethods* XTypeMaker::tp_as_number() {
  xassert(type);
  if (!type->tp_as_number) {
    auto num = new PyNumberMethods;
    num->nb_add = nullptr;
    num->nb_subtract = nullptr;
    num->nb_multiply = nullptr;
    num->nb_remainder = nullptr;
    num->nb_divmod = nullptr;
    num->nb_power = nullptr;
    num->nb_negative = nullptr;
    num->nb_positive = nullptr;
    num->nb_absolute = nullptr;
    num->nb_bool = nullptr;
    num->nb_invert = nullptr;
    num->nb_lshift = nullptr;
    num->nb_rshift = nullptr;
    num->nb_and = nullptr;
    num->nb_xor = nullptr;
    num->nb_or = nullptr;
    num->nb_int = nullptr;
    num->nb_float = nullptr;
    num->nb_inplace_add = nullptr;
    num->nb_inplace_subtract = nullptr;
    num->nb_inplace_multiply = nullptr;
    num->nb_inplace_remainder = nullptr;
    num->nb_inplace_power = nullptr;
    num->nb_inplace_lshift = nullptr;
    num->nb_inplace_rshift = nullptr;
    num->nb_inplace_and = nullptr;
    num->nb_inplace_xor = nullptr;
    num->nb_inplace_or = nullptr;
    num->nb_floor_divide = nullptr;
    num->nb_true_divide = nullptr;
    num->nb_inplace_floor_divide = nullptr;
    num->nb_inplace_true_divide = nullptr;
    num->nb_index = nullptr;
    num->nb_matrix_multiply = nullptr;
    num->nb_inplace_matrix_multiply = nullptr;
    type->tp_as_number = num;
  }
  return type->tp_as_number;
}



} // namespace py
