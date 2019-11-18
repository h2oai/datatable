//------------------------------------------------------------------------------
// Copyright 2019 H2O.ai
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
#include "expr/fbinary/bimaker.h"
#include "expr/fbinary/bimaker_impl.h"
namespace dt {
namespace expr {


static SType _find_common_stype(SType stype1, SType stype2) {
  while (stype1 != stype2) {
    if (stype1 == SType::VOID)    stype1 = stype2; else
    if (stype2 == SType::VOID)    stype2 = stype1; else
    if (stype1 == SType::BOOL)    stype1 = SType::INT8; else
    if (stype2 == SType::BOOL)    stype2 = SType::INT8; else
    if (stype1 == SType::INT8)    stype1 = SType::INT16; else
    if (stype2 == SType::INT8)    stype2 = SType::INT16; else
    if (stype1 == SType::INT16)   stype1 = SType::INT32; else
    if (stype2 == SType::INT16)   stype2 = SType::INT32; else
    if (stype1 == SType::INT32)   stype1 = SType::INT64; else
    if (stype2 == SType::INT32)   stype2 = SType::INT64; else
    if (stype1 == SType::INT64)   stype1 = SType::FLOAT32; else
    if (stype2 == SType::INT64)   stype2 = SType::FLOAT32; else
    if (stype1 == SType::FLOAT32) stype1 = SType::FLOAT64; else
    if (stype2 == SType::FLOAT32) stype2 = SType::FLOAT64; else
    if (stype1 == SType::STR32)   stype1 = SType::STR64; else
    if (stype2 == SType::STR32)   stype2 = SType::STR64; else
    return SType::INVALID;
  }
  return stype1;
}


/**
  * SType adjustment for operators `==` and `!=`. Numeric types are
  * promoted to the highest common stype, and string types to STR32.
  * Throws an error if `stype1` and `stype2` are incompatible (e.g.
  * a string and a number).
  */
static SType _find_types_for_eq(
    SType stype1, SType stype2, SType* uptype1, SType* uptype2,
    const char* name)
{
  SType stype0 = _find_common_stype(stype1, stype2);
  if (stype0 == SType::INVALID) {
    throw TypeError() << "Operator `" << name << "` cannot be applied to "
        "columns with types `" << stype1 << "` and `" << stype2 << "`";
  }
  if (stype0 == SType::STR32) stype0 = SType::STR64;
  if (stype0 == SType::STR64) {
    *uptype1 = *uptype2 = SType::VOID;
  } else {
    *uptype1 = (stype1 == stype0)? SType::VOID : stype0;
    *uptype2 = (stype2 == stype0)? SType::VOID : stype0;
  }
  return stype0;
}


/**
  * SType adjustment for comparison operators `<`, `>`, `<=` and `>=`.
  * Numeric types are promoted to largest among `stype1`, `stype2` and
  * INT32. String types are not supported.
  */
static SType _find_types_for_ltgt(
    SType stype1, SType stype2, SType* uptype1, SType* uptype2,
    const char* name)
{
  SType stype0 = _find_common_stype(stype1, stype2);
  if (stype0 == SType::INVALID || stype0 == SType::STR32 ||
      stype0 == SType::STR64)
  {
    throw TypeError() << "Operator `" << name << "` cannot be applied to "
        "columns with types `" << stype1 << "` and `" << stype2 << "`";
  }
  if (stype0 == SType::VOID || stype0 == SType::BOOL ||
      stype0 == SType::INT8 || stype0 == SType::INT16) {
    stype0 = SType::INT32;
  }
  *uptype1 = (stype1 == stype0)? SType::VOID : stype0;
  *uptype2 = (stype2 == stype0)? SType::VOID : stype0;
  return stype0;
}




//------------------------------------------------------------------------------
// Op::EQ  (==)
//------------------------------------------------------------------------------

// Note: for `CString`s the == operator is defined too
template <typename T>
static bool op_eq(T x, bool xvalid, T y, bool yvalid, int8_t* out) {
  *out = (xvalid == yvalid) && (x == y || !xvalid);
  return true;
}


template <typename T>
static inline bimaker_ptr _eq(SType uptype1, SType uptype2) {
  return bimaker2<T, T, int8_t>::make(op_eq<typename _ref<T>::t>,
                                      uptype1, uptype2, SType::BOOL);
}


// TODO: add specialization for SType::VOID (i.e. `f.X == None` should
//       be equivalent to `isna(f.X)`).
bimaker_ptr resolve_op_eq(SType stype1, SType stype2)
{
  SType uptype1, uptype2;
  SType stype0 = _find_types_for_eq(stype1, stype2, &uptype1, &uptype2, "==");
  switch (stype0) {
    case SType::BOOL:
    case SType::INT8:    return _eq<int8_t>(uptype1, uptype2);
    case SType::INT16:   return _eq<int16_t>(uptype1, uptype2);
    case SType::INT32:   return _eq<int32_t>(uptype1, uptype2);
    case SType::INT64:   return _eq<int64_t>(uptype1, uptype2);
    case SType::FLOAT32: return _eq<float>(uptype1, uptype2);
    case SType::FLOAT64: return _eq<double>(uptype1, uptype2);
    case SType::STR64:   return _eq<CString>(uptype1, uptype2);
    default:             return bimaker_ptr();
  }
}




//------------------------------------------------------------------------------
// Op::NE  (!=)
//------------------------------------------------------------------------------

// Note: for CString-s the `==` operator is defined too
template <typename T>
static bool op_ne(T x, bool xvalid, T y, bool yvalid, int8_t* out) {
  *out = (xvalid != yvalid) || (!(x == y) && xvalid);
  return true;
}


template <typename T>
static inline bimaker_ptr _ne(SType uptype1, SType uptype2) {
  return bimaker2<T, T, int8_t>::make(op_ne<typename _ref<T>::t>,
                                      uptype1, uptype2, SType::BOOL);
}


// TODO: add specialization for SType::VOID (i.e. `f.X != None` should
//       be equivalent to `~isna(f.X)`).
bimaker_ptr resolve_op_ne(SType stype1, SType stype2)
{
  SType uptype1, uptype2;
  SType stype0 = _find_types_for_eq(stype1, stype2, &uptype1, &uptype2, "!=");
  switch (stype0) {
    case SType::BOOL:
    case SType::INT8:    return _ne<int8_t>(uptype1, uptype2);
    case SType::INT16:   return _ne<int16_t>(uptype1, uptype2);
    case SType::INT32:   return _ne<int32_t>(uptype1, uptype2);
    case SType::INT64:   return _ne<int64_t>(uptype1, uptype2);
    case SType::FLOAT32: return _ne<float>(uptype1, uptype2);
    case SType::FLOAT64: return _ne<double>(uptype1, uptype2);
    case SType::STR64:   return _ne<CString>(uptype1, uptype2);
    default:             return bimaker_ptr();
  }
}




//------------------------------------------------------------------------------
// Op::LT  (<)
//------------------------------------------------------------------------------

template <typename T>
static bool op_lt(T x, bool xvalid, T y, bool yvalid, int8_t* out) {
  *out = (xvalid && yvalid && x < y);
  return true;
}


template <typename T>
static inline bimaker_ptr _lt(SType uptype1, SType uptype2) {
  return bimaker2<T, T, int8_t>::make(op_lt<typename _ref<T>::t>,
                                      uptype1, uptype2, SType::BOOL);
}


bimaker_ptr resolve_op_lt(SType stype1, SType stype2)
{
  SType uptype1, uptype2;
  SType stype0 = _find_types_for_ltgt(stype1, stype2, &uptype1, &uptype2, "<");
  switch (stype0) {
    case SType::INT32:   return _lt<int32_t>(uptype1, uptype2);
    case SType::INT64:   return _lt<int64_t>(uptype1, uptype2);
    case SType::FLOAT32: return _lt<float>(uptype1, uptype2);
    case SType::FLOAT64: return _lt<double>(uptype1, uptype2);
    default:             return bimaker_ptr();
  }
}




//------------------------------------------------------------------------------
// Op::GT  (>)
//------------------------------------------------------------------------------

template <typename T>
static bool op_gt(T x, bool xvalid, T y, bool yvalid, int8_t* out) {
  *out = (xvalid && yvalid && x > y);
  return true;
}


template <typename T>
static inline bimaker_ptr _gt(SType uptype1, SType uptype2) {
  return bimaker2<T, T, int8_t>::make(op_gt<typename _ref<T>::t>,
                                      uptype1, uptype2, SType::BOOL);
}


bimaker_ptr resolve_op_gt(SType stype1, SType stype2)
{
  SType uptype1, uptype2;
  SType stype0 = _find_types_for_ltgt(stype1, stype2, &uptype1, &uptype2, ">");
  switch (stype0) {
    case SType::INT32:   return _gt<int32_t>(uptype1, uptype2);
    case SType::INT64:   return _gt<int64_t>(uptype1, uptype2);
    case SType::FLOAT32: return _gt<float>(uptype1, uptype2);
    case SType::FLOAT64: return _gt<double>(uptype1, uptype2);
    default:             return bimaker_ptr();
  }
}




//------------------------------------------------------------------------------
// Op::LE  (<=)
//------------------------------------------------------------------------------

template <typename T>
static bool op_le(T x, bool xvalid, T y, bool yvalid, int8_t* out) {
  *out = (xvalid && yvalid && x <= y) || (!xvalid && !yvalid);
  return true;
}


template <typename T>
static inline bimaker_ptr _le(SType uptype1, SType uptype2) {
  return bimaker2<T, T, int8_t>::make(op_le<typename _ref<T>::t>,
                                      uptype1, uptype2, SType::BOOL);
}


bimaker_ptr resolve_op_le(SType stype1, SType stype2)
{
  SType uptype1, uptype2;
  SType stype0 = _find_types_for_ltgt(stype1, stype2, &uptype1, &uptype2, "<=");
  switch (stype0) {
    case SType::INT32:   return _le<int32_t>(uptype1, uptype2);
    case SType::INT64:   return _le<int64_t>(uptype1, uptype2);
    case SType::FLOAT32: return _le<float>(uptype1, uptype2);
    case SType::FLOAT64: return _le<double>(uptype1, uptype2);
    default:             return bimaker_ptr();
  }
}




//------------------------------------------------------------------------------
// Op::GE  (>=)
//------------------------------------------------------------------------------

template <typename T>
static bool op_ge(T x, bool xvalid, T y, bool yvalid, int8_t* out) {
  *out = (xvalid && yvalid && x >= y) || (!xvalid && !yvalid);
  return true;
}


template <typename T>
static inline bimaker_ptr _ge(SType uptype1, SType uptype2) {
  return bimaker2<T, T, int8_t>::make(op_ge<typename _ref<T>::t>,
                                      uptype1, uptype2, SType::BOOL);
}


bimaker_ptr resolve_op_ge(SType stype1, SType stype2)
{
  SType uptype1, uptype2;
  SType stype0 = _find_types_for_ltgt(stype1, stype2, &uptype1, &uptype2, ">=");
  switch (stype0) {
    case SType::INT32:   return _ge<int32_t>(uptype1, uptype2);
    case SType::INT64:   return _ge<int64_t>(uptype1, uptype2);
    case SType::FLOAT32: return _ge<float>(uptype1, uptype2);
    case SType::FLOAT64: return _ge<double>(uptype1, uptype2);
    default:             return bimaker_ptr();
  }
}




}}  // namespace dt::expr
