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
//  AND, OR, XOR
//  LSHIFT, RSHIFT
//------------------------------------------------------------------------------
#include "expr/fbinary/bimaker.h"
#include "expr/fbinary/bimaker_impl.h"
namespace dt {
namespace expr {


static SType _find_common_stype(SType stype1, SType stype2) {
  while (stype1 != stype2) {
    if (stype1 == SType::VOID)    stype1 = SType::BOOL; else
    if (stype2 == SType::VOID)    stype2 = SType::BOOL; else
    if (stype1 == SType::BOOL)    stype1 = SType::INT8; else
    if (stype2 == SType::BOOL)    stype2 = SType::INT8; else
    if (stype1 == SType::INT8)    stype1 = SType::INT16; else
    if (stype2 == SType::INT8)    stype2 = SType::INT16; else
    if (stype1 == SType::INT16)   stype1 = SType::INT32; else
    if (stype2 == SType::INT16)   stype2 = SType::INT32; else
    if (stype1 == SType::INT32)   stype1 = SType::INT64; else
    if (stype2 == SType::INT32)   stype2 = SType::INT64; else
    return SType::INVALID;
  }
  return stype1;
}


/**
  * Find suitable common stype for logical operations AND, OR, XOR.
  * If both operands are boolean then the common stype will also be
  * BOOL. If both operands are integer (one may also be boolean),
  * then the common stype will be the largest of two integer stypes.
  * Floating-point and string stypes are not allowed.
  */
static SType _find_types_for_andor(
    SType stype1, SType stype2, SType* uptype1, SType* uptype2,
    const char* name)
{
  SType stype0 = _find_common_stype(stype1, stype2);
  LType ltype0 = ::info(stype0).ltype();
  if (!(ltype0 == LType::BOOL || ltype0 == LType::INT)) {
    throw TypeError() << "Operator `" << name << "` cannot be applied to "
        "columns with types `" << stype1 << "` and `" << stype2 << "`";
  }
  *uptype1 = (stype1 == stype0)? SType::VOID : stype0;
  *uptype2 = (stype2 == stype0)? SType::VOID : stype0;
  return stype0;
}





//------------------------------------------------------------------------------
// Op::AND  (&)
//------------------------------------------------------------------------------

template <typename T>
inline static T op_and(T x, T y) {
  return (x & y);
}


template <typename T>
static inline bimaker_ptr _and(SType uptype1, SType uptype2, SType outtype) {
  return bimaker1<T, T, T>::make(op_and<T>, uptype1, uptype2, outtype);
}


bimaker_ptr resolve_op_and(SType stype1, SType stype2)
{
  SType uptype1, uptype2;
  SType stype0 = _find_types_for_andor(stype1, stype2, &uptype1, &uptype2, "&");
  switch (stype0) {
    case SType::BOOL:  return _and<int8_t>(uptype1, uptype2, stype0);
    case SType::INT8:  return _and<int8_t>(uptype1, uptype2, stype0);
    case SType::INT16: return _and<int16_t>(uptype1, uptype2, stype0);
    case SType::INT32: return _and<int32_t>(uptype1, uptype2, stype0);
    case SType::INT64: return _and<int64_t>(uptype1, uptype2, stype0);
    default:           return bimaker_ptr();
  }
}




//------------------------------------------------------------------------------
// Op::OR  (|)
//------------------------------------------------------------------------------

template <typename T>
inline static T op_or(T x, T y) {
  return (x | y);
}


template <typename T>
static inline bimaker_ptr _or(SType uptype1, SType uptype2, SType outtype) {
  assert_compatible_type<T>(outtype);
  if (uptype1 != SType::VOID) assert_compatible_type<T>(uptype1);
  if (uptype1 != SType::VOID) assert_compatible_type<T>(uptype2);
  return bimaker1<T, T, T>::make(op_or<T>, uptype1, uptype2, outtype);
}


bimaker_ptr resolve_op_or(SType stype1, SType stype2)
{
  SType uptype1, uptype2;
  SType stype0 = _find_types_for_andor(stype1, stype2, &uptype1, &uptype2, "|");
  switch (stype0) {
    case SType::BOOL:  return _or<int8_t>(uptype1, uptype2, stype0);
    case SType::INT8:  return _or<int8_t>(uptype1, uptype2, stype0);
    case SType::INT16: return _or<int16_t>(uptype1, uptype2, stype0);
    case SType::INT32: return _or<int32_t>(uptype1, uptype2, stype0);
    case SType::INT64: return _or<int64_t>(uptype1, uptype2, stype0);
    default:           return bimaker_ptr();
  }
}




//------------------------------------------------------------------------------
// Op::XOR  (^)
//------------------------------------------------------------------------------

template <typename T>
inline static T op_xor(T x, T y) {
  return (x ^ y);
}


template <typename T>
static inline bimaker_ptr _xor(SType uptype1, SType uptype2, SType outtype) {
  assert_compatible_type<T>(outtype);
  if (uptype1 != SType::VOID) assert_compatible_type<T>(uptype1);
  if (uptype1 != SType::VOID) assert_compatible_type<T>(uptype2);
  return bimaker1<T, T, T>::make(op_xor<T>, uptype1, uptype2, outtype);
}


bimaker_ptr resolve_op_xor(SType stype1, SType stype2)
{
  SType uptype1, uptype2;
  SType stype0 = _find_types_for_andor(stype1, stype2, &uptype1, &uptype2, "^");
  switch (stype0) {
    case SType::BOOL:  return _xor<int8_t>(uptype1, uptype2, stype0);
    case SType::INT8:  return _xor<int8_t>(uptype1, uptype2, stype0);
    case SType::INT16: return _xor<int16_t>(uptype1, uptype2, stype0);
    case SType::INT32: return _xor<int32_t>(uptype1, uptype2, stype0);
    case SType::INT64: return _xor<int64_t>(uptype1, uptype2, stype0);
    default:           return bimaker_ptr();
  }
}




}}  // namespace dt::expr
