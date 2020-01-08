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
#ifndef dt_COLUMN_FUNC_BINARY_h
#define dt_COLUMN_FUNC_BINARY_h
#include "column.h"
#include "column/virtual.h"
#include "models/utils.h"
namespace dt {


/**
  * Virtual column obtained by applying a simple binary function
  * to a pair of columns `arg1_` and `arg2_`.
  *
  * The "simple function" has the form `(T1, T2) -> TO`, i.e. two
  * input values of types T1 and T2 are mapped into an output value
  * of type TO. If either the first or the second argument is NA,
  * then the result will be NA too (the function doesn't have to
  * handle this case). The value returned by `func_` must not be NA.
  *
  * If you have a binary function that does not satisfy these
  * constrains, please use the `FuncBinary2_ColumnImpl` class.
  */
template <typename T1, typename T2, typename TO>
class FuncBinary1_ColumnImpl : public Virtual_ColumnImpl {
  using R1 = typename _ref<T1>::t;
  using R2 = typename _ref<T2>::t;
  using func_t = TO(*)(R1, R2);
  protected:
    Column arg1_;
    Column arg2_;
    func_t func_;

  public:
    FuncBinary1_ColumnImpl(Column&&, Column&&, func_t, size_t nrows, SType);

    ColumnImpl* clone() const override;
    void verify_integrity() const override;
    bool allow_parallel_access() const override;

    bool get_element(size_t i, TO* out) const override;
};



/**
  * Similar to `FuncBinary1_ColumnImpl`, but the operating function
  * allows for special processing of NA values. Specifically, it
  * accepts the binary function with the signature
  *
  *   (T1 x1, bool x1_isvalid, T2 x2, bool x2_isvalid, TO* out) -> bool
  *
  */
template <typename T1, typename T2, typename TO>
class FuncBinary2_ColumnImpl : public Virtual_ColumnImpl {
  using R1 = typename _ref<T1>::t;
  using R2 = typename _ref<T2>::t;
  using func_t = bool(*)(R1, bool, R2, bool, TO*);
  protected:
    Column arg1_;
    Column arg2_;
    func_t func_;

  public:
    FuncBinary2_ColumnImpl(Column&&, Column&&, func_t, size_t nrows, SType);

    ColumnImpl* clone() const override;
    void verify_integrity() const override;
    bool allow_parallel_access() const override;

    bool get_element(size_t i, TO* out) const override;
};




//------------------------------------------------------------------------------
// FuncBinary1_ColumnImpl
//------------------------------------------------------------------------------

template <typename T1, typename T2, typename TO>
FuncBinary1_ColumnImpl<T1, T2, TO>::FuncBinary1_ColumnImpl(
    Column&& col1, Column&& col2, func_t f, size_t nrows, SType stype
)
  : Virtual_ColumnImpl(nrows, stype),
    arg1_(std::move(col1)),
    arg2_(std::move(col2)),
    func_(f)
{
  assert_compatible_type<TO>(stype);
  xassert(arg1_.nrows() == arg2_.nrows());
  xassert(nrows <= arg1_.nrows());
}


template <typename T1, typename T2, typename TO>
ColumnImpl* FuncBinary1_ColumnImpl<T1, T2, TO>::clone() const {
  return new FuncBinary1_ColumnImpl<T1, T2, TO>(
                Column(arg1_), Column(arg2_), func_, nrows_, stype_);
}


template <typename T1, typename T2, typename TO>
bool FuncBinary1_ColumnImpl<T1, T2, TO>::get_element(size_t i, TO* out) const {
  T1 x1; bool x1valid = arg1_.get_element(i, &x1);
  T2 x2; bool x2valid = arg2_.get_element(i, &x2);
  if (!x1valid || !x2valid) return false;
  TO value = func_(x1, x2);
  *out = value;
  return _notnan(value);
}


template <typename T1, typename T2, typename TO>
void FuncBinary1_ColumnImpl<T1, T2, TO>::verify_integrity() const {
  arg1_.verify_integrity();
  arg2_.verify_integrity();
  assert_compatible_type<TO>(stype_);
  assert_compatible_type<T1>(arg1_.stype());
  assert_compatible_type<T2>(arg2_.stype());
  XAssert(nrows_ <= arg2_.nrows());
  XAssert(nrows_ <= arg1_.nrows());
  XAssert(func_ != nullptr);
}


template <typename T1, typename T2, typename TO>
bool FuncBinary1_ColumnImpl<T1, T2, TO>::allow_parallel_access() const {
  return arg1_.allow_parallel_access() && arg2_.allow_parallel_access();
}




//------------------------------------------------------------------------------
// FuncBinary2_ColumnImpl
//------------------------------------------------------------------------------

template <typename T1, typename T2, typename TO>
FuncBinary2_ColumnImpl<T1, T2, TO>::FuncBinary2_ColumnImpl(
    Column&& col1, Column&& col2, func_t f, size_t nrows, SType stype
)
  : Virtual_ColumnImpl(nrows, stype),
    arg1_(std::move(col1)),
    arg2_(std::move(col2)),
    func_(f)
{
  assert_compatible_type<TO>(stype);
  xassert(arg1_.nrows() == arg2_.nrows());
  xassert(nrows <= arg1_.nrows());
}


template <typename T1, typename T2, typename TO>
ColumnImpl* FuncBinary2_ColumnImpl<T1, T2, TO>::clone() const {
  return new FuncBinary2_ColumnImpl<T1, T2, TO>(
                Column(arg1_), Column(arg2_), func_, nrows_, stype_);
}

template <typename T1, typename T2, typename TO>
bool FuncBinary2_ColumnImpl<T1, T2, TO>::get_element(size_t i, TO* out) const {
  T1 x1; bool x1valid = arg1_.get_element(i, &x1);
  T2 x2; bool x2valid = arg2_.get_element(i, &x2);
  return func_(x1, x1valid, x2, x2valid, out);
}


template <typename T1, typename T2, typename TO>
void FuncBinary2_ColumnImpl<T1, T2, TO>::verify_integrity() const {
  arg1_.verify_integrity();
  arg2_.verify_integrity();
  assert_compatible_type<TO>(stype_);
  assert_compatible_type<T1>(arg1_.stype());
  assert_compatible_type<T2>(arg2_.stype());
  XAssert(nrows_ <= arg2_.nrows());
  XAssert(nrows_ <= arg1_.nrows());
  XAssert(func_ != nullptr);
}


template <typename T1, typename T2, typename TO>
bool FuncBinary2_ColumnImpl<T1, T2, TO>::allow_parallel_access() const {
  return arg1_.allow_parallel_access() && arg2_.allow_parallel_access();
}




}  // namespace dt
#endif
