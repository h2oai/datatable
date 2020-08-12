//------------------------------------------------------------------------------
// Copyright 2019-2020 H2O.ai
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


// lenfunc = Py_ssize_t(*)(PyObject*)
void XTypeMaker::add(lenfunc _length, LengthTag) {
  init_tp_as_mapping();
  type->tp_as_mapping->mp_length = _length;
}


// getattrofunc = PyObject*(*)(PyObject*, PyObject*)
void XTypeMaker::add(getattrofunc getattr, GetattrTag) {
  type->tp_getattro = getattr;
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


// ternaryfunc = PyObject*(*)(PyObject*, PyObject*, PyObject*)
void XTypeMaker::add(ternaryfunc meth, CallTag) {
  type->tp_call = meth;
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


PyNumberMethods* XTypeMaker::tp_as_number() {
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
