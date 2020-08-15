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
#include <cmath>                          // std::pow, std::powf
#include <limits>                         // std::numeric_limits
#include "column/string_plus.h"           // StringPlus_ColumnImpl
#include "expr/fbinary/bimaker.h"
#include "expr/fbinary/bimaker_impl.h"
#include "ltype.h"
namespace dt {
namespace expr {




//------------------------------------------------------------------------------
// Op::MODULO (%)
//------------------------------------------------------------------------------

template <typename T>
inline static bool op_modulo(T x, bool xvalid, T y, bool yvalid, T* out) {
  if (!xvalid || !yvalid || y == 0) return false;
  T res = x % y;
  if ((x < 0) != (y < 0) && res != 0) {
    res += y;
  }
  *out = res;
  return true;
}


template <typename T>
static inline bimaker_ptr _modulo(SType uptype1, SType uptype2, SType outtype) {
  xassert(compatible_type<T>(outtype));
  if (uptype1 != SType::VOID) xassert(compatible_type<T>(uptype1));
  if (uptype2 != SType::VOID) xassert(compatible_type<T>(uptype2));
  return bimaker2<T, T, T>::make(op_modulo<T>, uptype1, uptype2, outtype);
}


/**
  * Operator `%` implements the following rules:
  *
  *   VOID // {*} -> VOID
  *   {BOOL, INT8, INT16, INT32} % {BOOL, IN8, INT16, INT32} -> INT32
  *   INT64 % {BOOL, INT*} -> INT64
  *   FLOAT32 % {BOOL, INT*, FLOAT32} -> FLOAT32  [not implemented yet]
  *   FLOAT64 % {BOOL, INT*, FLOAT*} -> FLOAT64   [not implemented yet]
  *
  */
bimaker_ptr resolve_op_modulo(SType stype1, SType stype2)
{
  if (stype1 == SType::VOID || stype2 == SType::VOID) {
    return bimaker_nacol::make();
  }
  SType stype0 = common_stype(stype1, stype2);
  if (stype0 == SType::BOOL || stype0 == SType::INT8 || stype0 == SType::INT16) {
    stype0 = SType::INT32;
  }
  SType uptype1 = (stype1 == stype0)? SType::VOID : stype0;
  SType uptype2 = (stype2 == stype0)? SType::VOID : stype0;
  switch (stype0) {
    case SType::INT32:   return _modulo<int32_t>(uptype1, uptype2, stype0);
    case SType::INT64:   return _modulo<int64_t>(uptype1, uptype2, stype0);
    default:
      throw TypeError() << "Operator `%` cannot be applied to columns of "
        "types `" << stype1 << "` and `" << stype2 << "`";
  }
}




//------------------------------------------------------------------------------
// Op::POWEROP (**)
//------------------------------------------------------------------------------

template <typename T>
inline T op_power(T x, T y) {
  return static_cast<T>(std::pow(static_cast<double>(x),
                                 static_cast<double>(y)));
}

template <>
inline float op_power(float x, float y) {
  return std::pow(x, y);
}


template <typename T>
static inline bimaker_ptr _power(SType uptype1, SType uptype2, SType outtype) {
  xassert(compatible_type<T>(outtype));
  if (uptype1 != SType::VOID) xassert(compatible_type<T>(uptype1));
  if (uptype2 != SType::VOID) xassert(compatible_type<T>(uptype2));
  return bimaker1<T, T, T>::make(op_power<T>, uptype1, uptype2, outtype);
}


/**
  * Operator `**` implements the following rules:
  *
  *   VOID ** {*} -> VOID
  *   {BOOL, INT8, INT16, INT32} ** {same} -> INT32
  *   INT64 ** {BOOL, INT*} -> INT64
  *   FLOAT32 ** {BOOL, INT*, FLOAT32} -> FLOAT32
  *   FLOAT64 ** {BOOL, INT*, FLOAT*} -> FLOAT64
  *
  * Note that these rules imply that `2 ** -2 == 0`, while
  * `2 ** -2.0 == 0.25`. This may be somewhat counterintuitive if the
  * user tries to compute `f.A ** -1` for example. In this case we
  * recommend to write `f.A ** -1.0` instead. In the future we may be
  * able to automatically apply such fix, in case when the power is
  * a negative python integer.
  *
  */
bimaker_ptr resolve_op_power(SType stype1, SType stype2)
{
  if (stype1 == SType::VOID || stype2 == SType::VOID) {
    return bimaker_nacol::make();
  }
  SType stype0 = common_stype(stype1, stype2);
  if (stype0 == SType::BOOL || stype0 == SType::INT8 || stype0 == SType::INT16) {
    stype0 = SType::INT32;
  }
  SType uptype1 = (stype1 == stype0)? SType::VOID : stype0;
  SType uptype2 = (stype2 == stype0)? SType::VOID : stype0;
  switch (stype0) {
    case SType::INT32:   return _power<int32_t>(uptype1, uptype2, stype0);
    case SType::INT64:   return _power<int64_t>(uptype1, uptype2, stype0);
    case SType::FLOAT32: return _power<float>(uptype1, uptype2, stype0);
    case SType::FLOAT64: return _power<double>(uptype1, uptype2, stype0);
    default:
      throw TypeError() << "Operator `**` cannot be applied to columns of "
        "types `" << stype1 << "` and `" << stype2 << "`";
  }
}




}}  // namespace dt::expr
