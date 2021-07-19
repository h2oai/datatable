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
#include "documentation.h"
#include "expr/fbinary/bimaker.h"
#include "expr/fbinary/bimaker_impl.h"
#include "ltype.h"
#include "python/args.h"
#include "utils/macros.h"
namespace dt {
namespace expr {



static SType _resolve_math_stypes(SType stype1, SType stype2,
                                  SType* uptype1, SType* uptype2)
{
  SType stype0 = common_stype(stype1, stype2);
  if (stype0 == SType::BOOL || stype_to_ltype(stype0) == LType::INT) {
    stype0 = SType::FLOAT64;
  }
  *uptype1 = (stype0 == stype1)? SType::AUTO : stype0;
  *uptype2 = (stype0 == stype2)? SType::AUTO : stype0;
  return stype0;
}



//------------------------------------------------------------------------------
// Op::ARCTAN2
//------------------------------------------------------------------------------

py::PKArgs args_atan2(2, 0, 0, false, false, {"x", "y"}, "atan2", doc_math_atan2);


template <typename T>
static bimaker_ptr _atan2(SType uptype1, SType uptype2, SType outtype) {
  return bimaker1<T, T, T>::make(std::atan2, uptype1, uptype2, outtype);
}


bimaker_ptr resolve_fn_atan2(SType stype1, SType stype2) {
  SType uptype1, uptype2;
  SType stype0 = _resolve_math_stypes(stype1, stype2, &uptype1, &uptype2);

  switch (stype0) {
    case SType::FLOAT32:  return _atan2<float>(uptype1, uptype2, stype0);
    case SType::FLOAT64:  return _atan2<double>(uptype1, uptype2, stype0);
    default:
      throw TypeError() << "Cannot apply function `atan2()` to columns with "
          "types `" << stype1 << "` and `" << stype2 << "`";
  }
}




//------------------------------------------------------------------------------
// Op::HYPOT
//------------------------------------------------------------------------------

py::PKArgs args_hypot(2, 0, 0, false, false, {"x", "y"}, "hypot", doc_math_hypot);


template <typename T>
static bimaker_ptr _hypot(SType uptype1, SType uptype2, SType outtype) {
  return bimaker1<T, T, T>::make(std::hypot, uptype1, uptype2, outtype);
}


bimaker_ptr resolve_fn_hypot(SType stype1, SType stype2) {
  SType uptype1, uptype2;
  SType stype0 = _resolve_math_stypes(stype1, stype2, &uptype1, &uptype2);
  switch (stype0) {
    case SType::FLOAT32:  return _hypot<float>(uptype1, uptype2, stype0);
    case SType::FLOAT64:  return _hypot<double>(uptype1, uptype2, stype0);
    default:
      throw TypeError() << "Cannot apply function `hypot()` to columns with "
          "types `" << stype1 << "` and `" << stype2 << "`";
  }
}




//------------------------------------------------------------------------------
// Op::POWERFN
//------------------------------------------------------------------------------

py::PKArgs args_pow(2, 0, 0, false, false, {"x", "y"}, "pow", doc_math_pow);


template <typename T>
static bimaker_ptr _pow(SType uptype1, SType uptype2, SType outtype) {
  return bimaker1<T, T, T>::make(std::pow, uptype1, uptype2, outtype);
}


bimaker_ptr resolve_fn_pow(SType stype1, SType stype2) {
  SType uptype1, uptype2;
  SType stype0 = _resolve_math_stypes(stype1, stype2, &uptype1, &uptype2);
  switch (stype0) {
    case SType::FLOAT32:  return _pow<float>(uptype1, uptype2, stype0);
    case SType::FLOAT64:  return _pow<double>(uptype1, uptype2, stype0);
    default:
      throw TypeError() << "Cannot apply function `pow()` to columns with "
          "types `" << stype1 << "` and `" << stype2 << "`";
  }
}




//------------------------------------------------------------------------------
// Op::COPYSIGN
//------------------------------------------------------------------------------

py::PKArgs args_copysign(2, 0, 0, false, false, {"x", "y"}, "copysign",
                         doc_math_copysign);

template <typename T>
static bimaker_ptr _copysign(SType uptype1, SType uptype2, SType outtype) {
  return bimaker1<T, T, T>::make(std::copysign, uptype1, uptype2, outtype);
}


bimaker_ptr resolve_fn_copysign(SType stype1, SType stype2) {
  SType uptype1, uptype2;
  SType stype0 = _resolve_math_stypes(stype1, stype2, &uptype1, &uptype2);
  switch (stype0) {
    case SType::FLOAT32:  return _copysign<float>(uptype1, uptype2, stype0);
    case SType::FLOAT64:  return _copysign<double>(uptype1, uptype2, stype0);
    default:
      throw TypeError() << "Cannot apply function `copysign()` to columns with "
          "types `" << stype1 << "` and `" << stype2 << "`";
  }
}




//------------------------------------------------------------------------------
// Op::LOGADDEXP
//------------------------------------------------------------------------------

py::PKArgs args_logaddexp(2, 0, 0, false, false, {"x", "y"}, "logaddexp",
                          doc_math_logaddexp);

template <typename T>
static T op_logaddexp(T x, T y) {
  static constexpr T LN2 = T(0.6931471805599453);
  if (x == y) return x + LN2;
  T delta = x - y;
  return (delta >= 0)? x + std::log1p(std::exp(-delta))
                     : y + std::log1p(std::exp(delta));
}


template <typename T>
static bimaker_ptr _logaddexp(SType uptype1, SType uptype2, SType outtype) {
  return bimaker1<T, T, T>::make(op_logaddexp<T>, uptype1, uptype2, outtype);
}


bimaker_ptr resolve_fn_logaddexp(SType stype1, SType stype2) {
  SType uptype1, uptype2;
  SType stype0 = _resolve_math_stypes(stype1, stype2, &uptype1, &uptype2);
  switch (stype0) {
    case SType::FLOAT32:  return _logaddexp<float>(uptype1, uptype2, stype0);
    case SType::FLOAT64:  return _logaddexp<double>(uptype1, uptype2, stype0);
    default:
      throw TypeError() << "Cannot apply function `logaddexp()` to columns with "
          "types `" << stype1 << "` and `" << stype2 << "`";
  }
}




//------------------------------------------------------------------------------
// Op::LOGADDEXP2
//------------------------------------------------------------------------------

py::PKArgs args_logaddexp2(2, 0, 0, false, false, {"x", "y"}, "logaddexp2",
                           doc_math_logaddexp2);

template <typename T>
static T op_logaddexp2(T x, T y) {
  static constexpr T LN2INV = T(1.4426950408889634);
  if (x == y) return x + 1;
  T delta = x - y;
  return (delta >= 0)? x + std::log1p(std::exp2(-delta)) * LN2INV
                     : y + std::log1p(std::exp2(delta)) * LN2INV;
}


template <typename T>
static bimaker_ptr _logaddexp2(SType uptype1, SType uptype2, SType outtype) {
  return bimaker1<T, T, T>::make(op_logaddexp2<T>, uptype1, uptype2, outtype);
}


bimaker_ptr resolve_fn_logaddexp2(SType stype1, SType stype2) {
  SType uptype1, uptype2;
  SType stype0 = _resolve_math_stypes(stype1, stype2, &uptype1, &uptype2);
  switch (stype0) {
    case SType::FLOAT32:  return _logaddexp2<float>(uptype1, uptype2, stype0);
    case SType::FLOAT64:  return _logaddexp2<double>(uptype1, uptype2, stype0);
    default:
      throw TypeError() << "Cannot apply function `logaddexp2()` to columns with "
          "types `" << stype1 << "` and `" << stype2 << "`";
  }
}




//------------------------------------------------------------------------------
// Op::FMOD
//------------------------------------------------------------------------------

py::PKArgs args_fmod(2, 0, 0, false, false, {"x", "y"}, "fmod", doc_math_fmod);

template <typename T>
static bimaker_ptr _fmod(SType uptype1, SType uptype2, SType outtype) {
  return bimaker1<T, T, T>::make(std::fmod, uptype1, uptype2, outtype);
}


bimaker_ptr resolve_fn_fmod(SType stype1, SType stype2) {
  SType uptype1, uptype2;
  SType stype0 = _resolve_math_stypes(stype1, stype2, &uptype1, &uptype2);
  switch (stype0) {
    case SType::FLOAT32:  return _fmod<float>(uptype1, uptype2, stype0);
    case SType::FLOAT64:  return _fmod<double>(uptype1, uptype2, stype0);
    default:
      throw TypeError() << "Cannot apply function `fmod()` to columns with "
          "types `" << stype1 << "` and `" << stype2 << "`";
  }
}




//------------------------------------------------------------------------------
// Op::LDEXP
//------------------------------------------------------------------------------

py::PKArgs args_ldexp(2, 0, 0, false, false, {"x", "y"}, "ldexp",
                      doc_math_ldexp);

template <typename T>
static bimaker_ptr _ldexp(SType uptype1, SType uptype2, SType outtype) {
  return bimaker1<T, int32_t, T>::make(std::ldexp, uptype1, uptype2, outtype);
}


bimaker_ptr resolve_fn_ldexp(SType stype1, SType stype2) {
  SType out_stype = stype1;
  if (stype_to_ltype(out_stype) == LType::INT) out_stype = SType::FLOAT64;
  if (stype_to_ltype(stype2) != LType::INT)    out_stype = SType::INVALID;
  SType uptype1 = (stype1 == out_stype)? SType::AUTO : out_stype;
  SType uptype2 = (stype2 == SType::INT32)? SType::AUTO : SType::INT32;

  switch (out_stype) {
    case SType::FLOAT32:  return _ldexp<float>(uptype1, uptype2, out_stype);
    case SType::FLOAT64:  return _ldexp<double>(uptype1, uptype2, out_stype);
    default: break;
  }
  throw TypeError() << "Cannot apply function `ldexp()` to columns with "
      "types `" << stype1 << "` and `" << stype2 << "`";
}





}}  // namespace dt::expr
