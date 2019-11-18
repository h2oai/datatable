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



//------------------------------------------------------------------------------
// Op::PLUS (+)
//------------------------------------------------------------------------------

template <typename T>
inline static T op_plus(T x, T y) {
  return (x + y);
}


template <typename T>
static inline bimaker_ptr _plus(SType uptype1, SType uptype2, SType outtype) {
  assert_compatible_type<T>(outtype);
  if (uptype1 != SType::VOID) assert_compatible_type<T>(uptype1);
  if (uptype2 != SType::VOID) assert_compatible_type<T>(uptype2);
  return bimaker1<T, T, T>::make(op_plus<T>, uptype1, uptype2, outtype);
}


/**
  * Operator `+` implements the following rules:
  *
  *   VOID + {*} -> VOID
  *   {BOOL, INT8, INT16, INT32} + {BOOL, INT8, INT16, INT32} -> INT32
  *   INT64 + {BOOL, INT8, INT16, INT32, INT64} -> INT64
  *   FLOAT32 + {BOOL, INT*, FLOAT32} -> FLOAT32
  *   FLOAT64 + {BOOL, INT*, FLOAT*} -> FLOAT64
  *   {STR32, STR64} + {STR32, STR64} -> STR32  [not implemented yet]
  *
  */
bimaker_ptr resolve_op_plus(SType stype1, SType stype2)
{
  if (stype1 == SType::VOID || stype2 == SType::VOID) {
    return bimaker_nacol::make();
  }
  SType stype0 = _find_common_stype(stype1, stype2);
  if (stype0 == SType::BOOL || stype0 == SType::INT8 || stype0 == SType::INT16) {
    stype0 = SType::INT32;
  }
  if (stype0 == SType::STR32 || stype0 == SType::STR64) {
    throw NotImplError() << "Operator + for string columns not available yet";
  }
  SType uptype1 = (stype1 == stype0)? SType::VOID : stype0;
  SType uptype2 = (stype2 == stype0)? SType::VOID : stype0;
  switch (stype0) {
    case SType::INT32:   return _plus<int32_t>(uptype1, uptype2, stype0);
    case SType::INT64:   return _plus<int64_t>(uptype1, uptype2, stype0);
    case SType::FLOAT32: return _plus<float>(uptype1, uptype2, stype0);
    case SType::FLOAT64: return _plus<double>(uptype1, uptype2, stype0);
    default:
      throw TypeError() << "Operator `+` cannot be applied to columns of "
        "types `" << stype1 << "` and `" << stype2 << "`";
  }
}




//------------------------------------------------------------------------------
// Op::MINUS (-)
//------------------------------------------------------------------------------

template <typename T>
inline static T op_minus(T x, T y) {
  return (x - y);
}


template <typename T>
static inline bimaker_ptr _minus(SType uptype1, SType uptype2, SType outtype) {
  assert_compatible_type<T>(outtype);
  if (uptype1 != SType::VOID) assert_compatible_type<T>(uptype1);
  if (uptype2 != SType::VOID) assert_compatible_type<T>(uptype2);
  return bimaker1<T, T, T>::make(op_minus<T>, uptype1, uptype2, outtype);
}


/**
  * Operator `-` implements the following rules (the rules are
  * considered symmetrical in arguments x and y):
  *
  *   VOID - {*} -> VOID
  *   {BOOL, INT8, INT16, INT32} - {BOOL, INT8, INT16, INT32} -> INT32
  *   INT64 - {BOOL, INT8, INT16, INT32, INT64} -> INT64
  *   FLOAT32 - {BOOL, INT*, FLOAT32} -> FLOAT32
  *   FLOAT64 - {BOOL, INT*, FLOAT*} -> FLOAT64
  *
  */
bimaker_ptr resolve_op_minus(SType stype1, SType stype2)
{
  if (stype1 == SType::VOID || stype2 == SType::VOID) {
    return bimaker_nacol::make();
  }
  SType stype0 = _find_common_stype(stype1, stype2);
  if (stype0 == SType::BOOL || stype0 == SType::INT8 || stype0 == SType::INT16) {
    stype0 = SType::INT32;
  }
  SType uptype1 = (stype1 == stype0)? SType::VOID : stype0;
  SType uptype2 = (stype2 == stype0)? SType::VOID : stype0;
  switch (stype0) {
    case SType::INT32:   return _minus<int32_t>(uptype1, uptype2, stype0);
    case SType::INT64:   return _minus<int64_t>(uptype1, uptype2, stype0);
    case SType::FLOAT32: return _minus<float>(uptype1, uptype2, stype0);
    case SType::FLOAT64: return _minus<double>(uptype1, uptype2, stype0);
    default:
      throw TypeError() << "Operator `-` cannot be applied to columns of "
        "types `" << stype1 << "` and `" << stype2 << "`";
  }
}





}}  // namespace dt::expr
