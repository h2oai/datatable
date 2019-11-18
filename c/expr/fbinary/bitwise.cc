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


/**
  * Find suitable stype(s) for bitwise shift operation. The stype of
  * the result is always equal to `stype1` (which may only be integer)
  * and the first argument is never promoted. The second argument can
  * be either integer or boolean, and is always promoted into INT32.
  */
static void _find_types_for_shifts(
    SType stype1, SType stype2, SType* uptype2, const char* name)
{
  LType ltype1 = ::info(stype1).ltype();
  LType ltype2 = ::info(stype2).ltype();
  if (ltype1 == LType::INT && (ltype2 == LType::INT || ltype2 == LType::BOOL)) {
    *uptype2 = (stype2 == SType::INT32)? SType::VOID : SType::INT32;
  }
  else {
    throw TypeError() << "Operator `" << name << "` cannot be applied to "
        "columns with types `" << stype1 << "` and `" << stype2 << "`";
  }
}




//------------------------------------------------------------------------------
// Op::AND (boolean)
//------------------------------------------------------------------------------

/**
  * Virtual column implementing short-circuit boolean-AND evalation.
  * Specifically, if columns X and Y are boolean, then each value
  * x and y can be in one of 3 possible states: 0, 1 and NA. The
  * result of (x & y) is given by this table:
  *              y
  *    AND | 0 | 1 | NA
  *    ----+---+---+---
  *      0 | 0 | 0 |  0   <-- short-circuit
  *  x   1 | 0 | 1 | NA
  *     NA | 0 | NA| NA
  *
  * In particular, notice that `(0 & y) == 0` no matter what the
  * value of `y` is, including NA.
  *
  * Also, the evaluation uses short-circuit semantics: if `x`
  * evaluates to 0 (False), then `y` is not computed at all.
  */
class BooleanAnd_ColumnImpl : public Virtual_ColumnImpl {
  protected:
    Column arg1_;
    Column arg2_;

  public:
    BooleanAnd_ColumnImpl(Column&& col1, Column&& col2, size_t nrows)
      : Virtual_ColumnImpl(nrows, SType::BOOL),
        arg1_(std::move(col1)), arg2_(std::move(col2)) {}

    ColumnImpl* clone() const override {
      return new BooleanAnd_ColumnImpl(Column(arg1_), Column(arg2_), nrows_);
    }

    void verify_integrity() const override {
      XAssert(arg1_.stype() == SType::BOOL);
      XAssert(arg2_.stype() == SType::BOOL);
    }

    bool allow_parallel_access() const override {
      return arg1_.allow_parallel_access() && arg2_.allow_parallel_access();
    }

    bool get_element(size_t i, int8_t* out) const override {
      int8_t x, y;
      bool xvalid = arg1_.get_element(i, &x);
      if (x == 0 && xvalid) {  // short-circuit
        *out = 0;
        return true;
      }
      bool yvalid = arg2_.get_element(i, &y);
      if (!yvalid) return false;
      if (y == 0) {
        *out = 0;
        return true;
      }
      *out = 1;
      return xvalid;
    }
};


class BooleanAnd_bimaker : public bimaker {
  public:
    Column compute(Column&& col1, Column&& col2) const override {
      size_t nrows = col1.nrows();
      return Column(
          new BooleanAnd_ColumnImpl(std::move(col1), std::move(col2), nrows));
    }
};




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
  if (stype1 == SType::BOOL && stype2 == SType::BOOL) {
    return bimaker_ptr(new BooleanAnd_bimaker());
  }
  SType uptype1, uptype2;
  SType stype0 = _find_types_for_andor(stype1, stype2, &uptype1, &uptype2, "&");
  switch (stype0) {
    case SType::INT8:  return _and<int8_t>(uptype1, uptype2, stype0);
    case SType::INT16: return _and<int16_t>(uptype1, uptype2, stype0);
    case SType::INT32: return _and<int32_t>(uptype1, uptype2, stype0);
    case SType::INT64: return _and<int64_t>(uptype1, uptype2, stype0);
    default:           return bimaker_ptr();
  }
}




//------------------------------------------------------------------------------
// Op::OR (boolean)
//------------------------------------------------------------------------------

/**
  * Virtual column implementing short-circuit boolean-OR evalation.
  * Specifically, if columns X and Y are boolean, then each value
  * x and y can be in one of 3 possible states: 0, 1 and NA. The
  * result of (x | y) is given by this table:
  *              y
  *     OR |  0 | 1 | NA
  *    ----+----+---+---
  *      0 |  0 | 1 | NA
  *  x   1 |  1 | 1 |  1   <-- short-circuit
  *     NA | NA | 1 | NA
  *
  * In particular, notice that `(1 | y) == 1` no matter what the
  * value of `y` is, including NA.
  *
  * Also, the evaluation uses short-circuit semantics: if `x`
  * evaluates to 1 (True), then `y` is not computed at all.
  */
class BooleanOr_ColumnImpl : public Virtual_ColumnImpl {
  protected:
    Column arg1_;
    Column arg2_;

  public:
    BooleanOr_ColumnImpl(Column&& col1, Column&& col2, size_t nrows)
      : Virtual_ColumnImpl(nrows, SType::BOOL),
        arg1_(std::move(col1)), arg2_(std::move(col2)) {}

    ColumnImpl* clone() const override {
      return new BooleanOr_ColumnImpl(Column(arg1_), Column(arg2_), nrows_);
    }

    void verify_integrity() const override {
      XAssert(arg1_.stype() == SType::BOOL);
      XAssert(arg2_.stype() == SType::BOOL);
    }

    bool allow_parallel_access() const override {
      return arg1_.allow_parallel_access() && arg2_.allow_parallel_access();
    }

    bool get_element(size_t i, int8_t* out) const override {
      int8_t x, y;
      bool xvalid = arg1_.get_element(i, &x);
      if (x == 1 && xvalid) {  // short-circuit
        *out = 1;
        return true;
      }
      bool yvalid = arg2_.get_element(i, &y);
      if (!yvalid) return false;
      if (y == 1) {
        *out = 1;
        return true;
      }
      *out = 0;
      return xvalid;
    }
};


class BooleanOr_bimaker : public bimaker {
  public:
    Column compute(Column&& col1, Column&& col2) const override {
      size_t nrows = col1.nrows();
      return Column(
          new BooleanOr_ColumnImpl(std::move(col1), std::move(col2), nrows));
    }
};




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
  if (uptype2 != SType::VOID) assert_compatible_type<T>(uptype2);
  return bimaker1<T, T, T>::make(op_or<T>, uptype1, uptype2, outtype);
}


bimaker_ptr resolve_op_or(SType stype1, SType stype2)
{
  if (stype1 == SType::BOOL && stype2 == SType::BOOL) {
    return bimaker_ptr(new BooleanOr_bimaker());
  }
  SType uptype1, uptype2;
  SType stype0 = _find_types_for_andor(stype1, stype2, &uptype1, &uptype2, "|");
  switch (stype0) {
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
  if (uptype2 != SType::VOID) assert_compatible_type<T>(uptype2);
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




//------------------------------------------------------------------------------
// Op::LSHIFT  (<<)
//------------------------------------------------------------------------------

template <typename T>
inline static T op_lshift(T x, int32_t y) {
  return (y >= 0)? static_cast<T>(x << y) : static_cast<T>(x >> -y);
}


template <typename T>
static inline bimaker_ptr _lshift(SType outtype, SType uptype2) {
  assert_compatible_type<T>(outtype);
  return bimaker1<T, int32_t, T>::make(op_lshift<T>, SType::VOID,
                                       uptype2, outtype);
}


bimaker_ptr resolve_op_lshift(SType stype1, SType stype2)
{
  SType uptype2;
  _find_types_for_shifts(stype1, stype2, &uptype2, "<<");
  switch (stype1) {
    case SType::INT8:  return _lshift<int8_t>(stype1, uptype2);
    case SType::INT16: return _lshift<int16_t>(stype1, uptype2);
    case SType::INT32: return _lshift<int32_t>(stype1, uptype2);
    case SType::INT64: return _lshift<int64_t>(stype1, uptype2);
    default:           return bimaker_ptr();
  }
}




//------------------------------------------------------------------------------
// Op::RSHIFT  (>>)
//------------------------------------------------------------------------------

template <typename T>
inline static T op_rshift(T x, int32_t y) {
  return (y >= 0)? static_cast<T>(x >> y) : static_cast<T>(x << -y);
}


template <typename T>
static inline bimaker_ptr _rshift(SType outtype, SType uptype2) {
  assert_compatible_type<T>(outtype);
  return bimaker1<T, int32_t, T>::make(op_rshift<T>, SType::VOID,
                                       uptype2, outtype);
}


bimaker_ptr resolve_op_rshift(SType stype1, SType stype2)
{
  SType uptype2;
  _find_types_for_shifts(stype1, stype2, &uptype2, ">>");
  switch (stype1) {
    case SType::INT8:  return _rshift<int8_t>(stype1, uptype2);
    case SType::INT16: return _rshift<int16_t>(stype1, uptype2);
    case SType::INT32: return _rshift<int32_t>(stype1, uptype2);
    case SType::INT64: return _rshift<int64_t>(stype1, uptype2);
    default:           return bimaker_ptr();
  }
}




}}  // namespace dt::expr
