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
#include <cmath>
#include "documentation.h"
#include "expr/funary/pyfn.h"
#include "expr/funary/umaker.h"
#include "expr/funary/umaker_impl.h"
namespace dt {
namespace expr {



//------------------------------------------------------------------------------
// Op::ABS
//------------------------------------------------------------------------------

py::PKArgs args_abs(1, 0, 0, false, false, {"x"}, "abs", dt::doc_math_abs);

template <typename T>
static T op_abs(T x) {
  return (x >= 0)? x : -x;
}

template <typename T>
static inline umaker_ptr _abs(SType uptype = SType::AUTO) {
  return umaker1<T, T>::make(op_abs<T>, uptype, stype_from<T>);
}

umaker_ptr resolve_op_abs(SType stype) {
  switch (stype) {
    case SType::VOID:    return umaker_ptr(new umaker_nacol());
    case SType::BOOL:    return umaker_ptr(new umaker_cast(SType::INT32));
    case SType::INT8:
    case SType::INT16:   return _abs<int32_t>(/* uptype= */ SType::INT32);
    case SType::INT32:   return _abs<int32_t>();
    case SType::INT64:   return _abs<int64_t>();
    case SType::FLOAT32: return _abs<float>();
    case SType::FLOAT64: return _abs<double>();
    default:
      throw TypeError() << "Function `abs` cannot be applied to a "
                           "column of type `" << stype << "`";
  }
}




//------------------------------------------------------------------------------
// Op::FABS
//------------------------------------------------------------------------------

py::PKArgs args_fabs(1, 0, 0, false, false, {"x"}, "fabs", dt::doc_math_fabs);


umaker_ptr resolve_op_fabs(SType stype) {
  switch (stype) {
    case SType::VOID:    return umaker_ptr(new umaker_nacol());
    case SType::BOOL:    return umaker_ptr(new umaker_cast(SType::FLOAT64));
    case SType::INT8:
    case SType::INT16:
    case SType::INT32:
    case SType::INT64:   return _abs<double>(/* uptype= */ SType::FLOAT64);
    case SType::FLOAT32: return _abs<float>();
    case SType::FLOAT64: return _abs<double>();
    default:
      throw TypeError() << "Function `fabs` cannot be applied to a "
                           "column of type `" << stype << "`";
  }
}




//------------------------------------------------------------------------------
// Op::SIGN
//------------------------------------------------------------------------------

py::PKArgs args_sign(1, 0, 0, false, false, {"x"}, "sign", dt::doc_math_sign);


template <typename T>
static T op_sign(T x) {
  return (x > 0)? T(1) : (x < 0)? T(-1) : T(0);
}

template <typename T>
static inline umaker_ptr _sign(SType uptype = SType::AUTO) {
  return umaker1<T, T>::make(op_sign<T>, uptype, stype_from<T>);
}

umaker_ptr resolve_op_sign(SType stype) {
  switch (stype) {
    case SType::VOID:    return umaker_ptr(new umaker_nacol());
    case SType::BOOL:    return umaker_ptr(new umaker_cast(SType::FLOAT64));
    case SType::INT8:
    case SType::INT16:
    case SType::INT32:
    case SType::INT64:   return _sign<double>(SType::FLOAT64);
    case SType::FLOAT32: return _sign<float>();
    case SType::FLOAT64: return _sign<double>();
    default:
      throw TypeError() << "Function `sign` cannot be applied to a "
                           "column of type `" << stype << "`";
  }
}

//------------------------------------------------------------------------------
// Op::ISINF
//------------------------------------------------------------------------------

py::PKArgs args_isinf(1, 0, 0, false, false, {"x"}, "isinf", dt::doc_math_isinf);


template <typename T>
static bool op_isinf(T x, bool isvalid, int8_t* out) {
  *out = isvalid && std::isinf(x);
  return true;
}

template <typename T>
static inline umaker_ptr _isinf() {
  return umaker2<T, int8_t>::make(op_isinf<T>, SType::AUTO, SType::BOOL);
}

umaker_ptr resolve_op_isinf(SType stype) {
  switch (stype) {
    case SType::VOID:
    case SType::BOOL:
    case SType::INT8:
    case SType::INT16:
    case SType::INT32:
    case SType::INT64: {
      return umaker_ptr(new umaker_const(
                            Const_ColumnImpl::make_bool_column(1, false)));
    }
    case SType::FLOAT32: return _isinf<float>();
    case SType::FLOAT64: return _isinf<double>();
    default:
      throw TypeError() << "Function `isinf` cannot be applied to a "
                           "column of type `" << stype << "`";
  }
}




//------------------------------------------------------------------------------
// Op::ISFINITE
//------------------------------------------------------------------------------

py::PKArgs args_isfinite(1, 0, 0, false, false, {"x"}, "isfinite", dt::doc_math_isfinite);


template <typename T>
static bool op_isfinite(T x, bool xvalid, int8_t* out) {
  *out = xvalid && std::isfinite(x);
  return true;
}

template <typename T>
static bool op_notna(T, bool xvalid, int8_t* out) {
  *out = xvalid;
  return true;
}


template <typename T>
static umaker_ptr _isfinite_int(SType uptype = SType::AUTO) {
  return umaker2<T, int8_t>::make(op_notna<T>, uptype, SType::BOOL);
}

template <typename T>
static umaker_ptr _isfinite_float() {
  return umaker2<T, int8_t>::make(op_isfinite<T>, SType::AUTO, SType::BOOL);
}


umaker_ptr resolve_op_isfinite(SType stype) {
  switch (stype) {
    case SType::VOID: {
      return umaker_ptr(new umaker_const(
                            Const_ColumnImpl::make_bool_column(1, false)));
    }
    case SType::BOOL:
    case SType::INT8:
    case SType::INT16:   return _isfinite_int<int32_t>(SType::INT32);
    case SType::INT32:   return _isfinite_int<int32_t>();
    case SType::INT64:   return _isfinite_int<int64_t>();
    case SType::FLOAT32: return _isfinite_float<float>();
    case SType::FLOAT64: return _isfinite_float<double>();
    default:
      throw TypeError() << "Function `isfinite` cannot be applied to a "
                           "column of type `" << stype << "`";
  }
}




//------------------------------------------------------------------------------
// Op::CEIL
//------------------------------------------------------------------------------

py::PKArgs args_ceil(1, 0, 0, false, false, {"x"}, "ceil", dt::doc_math_ceil);


template <typename T>
static umaker_ptr _ceil() {
  return umaker1<T, T>::make(std::ceil, SType::AUTO, stype_from<T>);
}

umaker_ptr resolve_op_ceil(SType stype) {
  switch (stype) {
    case SType::VOID:    return umaker_ptr(new umaker_nacol());
    case SType::BOOL:
    case SType::INT8:
    case SType::INT16:
    case SType::INT32:
    case SType::INT64:   return umaker_ptr(new umaker_cast(SType::FLOAT64));
    case SType::FLOAT32: return _ceil<float>();
    case SType::FLOAT64: return _ceil<double>();
    default:
      throw TypeError() << "Function `ceil` cannot be applied to a "
                           "column of type `" << stype << "`";
  }
}




//------------------------------------------------------------------------------
// Op::FLOOR
//------------------------------------------------------------------------------

py::PKArgs args_floor(1, 0, 0, false, false, {"x"}, "floor", dt::doc_math_floor);


template <typename T>
static umaker_ptr _floor() {
  return umaker1<T, T>::make(std::floor, SType::AUTO, stype_from<T>);
}

umaker_ptr resolve_op_floor(SType stype) {
  switch (stype) {
    case SType::VOID:    return umaker_ptr(new umaker_nacol());
    case SType::BOOL:
    case SType::INT8:
    case SType::INT16:
    case SType::INT32:
    case SType::INT64:   return umaker_ptr(new umaker_cast(SType::FLOAT64));
    case SType::FLOAT32: return _floor<float>();
    case SType::FLOAT64: return _floor<double>();
    default:
      throw TypeError() << "Function `floor` cannot be applied to a "
                           "column of type `" << stype << "`";
  }
}




//------------------------------------------------------------------------------
// Op::RINT
//------------------------------------------------------------------------------

py::PKArgs args_rint(1, 0, 0, false, false, {"x"}, "rint", dt::doc_math_rint);


template <typename T>
static umaker_ptr _rint() {
  return umaker1<T, T>::make(std::rint, SType::AUTO, stype_from<T>);
}

umaker_ptr resolve_op_rint(SType stype) {
  switch (stype) {
    case SType::VOID:    return umaker_ptr(new umaker_nacol());
    case SType::BOOL:
    case SType::INT8:
    case SType::INT16:
    case SType::INT32:
    case SType::INT64:   return umaker_ptr(new umaker_cast(SType::FLOAT64));
    case SType::FLOAT32: return _rint<float>();
    case SType::FLOAT64: return _rint<double>();
    default:
      throw TypeError() << "Function `rint` cannot be applied to a "
                           "column of type `" << stype << "`";
  }
}




//------------------------------------------------------------------------------
// Op::TRUNC
//------------------------------------------------------------------------------

py::PKArgs args_trunc(1, 0, 0, false, false, {"x"}, "trunc", dt::doc_math_trunc);


template <typename T>
static umaker_ptr _trunc() {
  return umaker1<T, T>::make(std::trunc, SType::AUTO, stype_from<T>);
}

umaker_ptr resolve_op_trunc(SType stype) {
  switch (stype) {
    case SType::VOID:    return umaker_ptr(new umaker_nacol());
    case SType::BOOL:
    case SType::INT8:
    case SType::INT16:
    case SType::INT32:
    case SType::INT64:   return umaker_ptr(new umaker_cast(SType::FLOAT64));
    case SType::FLOAT32: return _trunc<float>();
    case SType::FLOAT64: return _trunc<double>();
    default:
      throw TypeError() << "Function `trunc` cannot be applied to a "
                           "column of type `" << stype << "`";
  }
}




//------------------------------------------------------------------------------
// Op::SIGNBIT
//------------------------------------------------------------------------------

py::PKArgs args_signbit(1, 0, 0, false, false, {"x"}, "signbit", dt::doc_math_signbit);


template <typename T>
static int8_t op_signbit(T x) { return std::signbit(x); }

template <typename T>
static umaker_ptr _signbit(SType uptype = SType::AUTO) {
  return umaker1<T, int8_t>::make(op_signbit<T>, uptype, SType::BOOL);
}

umaker_ptr resolve_op_signbit(SType stype) {
  switch (stype) {
    case SType::VOID:    return umaker_ptr(new umaker_nacol());
    case SType::BOOL:
    case SType::INT8:
    case SType::INT16:
    case SType::INT32:
    case SType::INT64:   return _signbit<double>(SType::FLOAT64);
    case SType::FLOAT32: return _signbit<float>();
    case SType::FLOAT64: return _signbit<double>();
    default:
      throw TypeError() << "Function `signbit` cannot be applied to a "
                           "column of type `" << stype << "`";
  }
}




}}  // namespace dt::expr
