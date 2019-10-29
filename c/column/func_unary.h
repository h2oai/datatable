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
#ifndef dt_COLUMN_FUNC_UNARY_h
#define dt_COLUMN_FUNC_UNARY_h
#include <cmath>
#include "column/virtual.h"
#include "column.h"
namespace dt {


/**
  * Virtual column obtained by applying a simple unary function
  * to another column `arg_`.
  *
  * The "simple function" has the form `TI -> TO`, i.e. a single
  * input value of type TI is mapped into an output value of type TO.
  * In addition, the function must implicitly map an NA value into
  * the NA output, and (with the exception of floating-point outputs)
  * may not produce NA value for an input that is not NA.
  *
  * If you have an unary function that does not satisfy these
  * constrains, please use the `FuncUnary2_ColumnImpl` class.
  */
template <typename TI, typename TO>
class FuncUnary1_ColumnImpl : public Virtual_ColumnImpl {
  using OP = TO(*)(TI);
  protected:
    Column arg_;
    OP func_;

  public:
    FuncUnary1_ColumnImpl(Column&&, OP);
    FuncUnary1_ColumnImpl(Column&&, OP, size_t nrows, SType stype);

    ColumnImpl* clone() const override;

    bool get_element(size_t i, TO* out) const override;
};


/**
  * Similar to `FuncUnary1_ColumnImpl`, but the operating function
  * allows for special processing of NA values. Specifically, it
  * accepts the unary function with the signature
  *
  *   (TI x, bool x_isvalid, TO* out) -> bool out_isvalid
  *
  */
template <typename TI, typename TO>
class FuncUnary2_ColumnImpl : public Virtual_ColumnImpl {
  using OP = bool(*)(TI, bool, TO*);
  protected:
    Column arg_;
    OP func_;

  public:
    FuncUnary2_ColumnImpl(Column&&, OP);
    FuncUnary2_ColumnImpl(Column&&, OP, size_t nrows, SType stype);

    ColumnImpl* clone() const override;

    bool get_element(size_t i, TO* out) const override;
};




//------------------------------------------------------------------------------
// FuncUnary1_ColumnImpl
//------------------------------------------------------------------------------

template<typename TI, typename TO>
FuncUnary1_ColumnImpl<TI, TO>::FuncUnary1_ColumnImpl(Column&& col, OP f)
  : FuncUnary1_ColumnImpl(std::move(col), f, col.nrows(), stype_from<TO>) {}


template<typename TI, typename TO>
FuncUnary1_ColumnImpl<TI, TO>::FuncUnary1_ColumnImpl(
    Column&& col, OP f, size_t nrows, SType stype
)
  : Virtual_ColumnImpl(nrows, stype),
    arg_(std::move(col)),
    func_(f)
{
  assert_compatible_type<TO>(stype);
}


template<typename TI, typename TO>
ColumnImpl* FuncUnary1_ColumnImpl<TI, TO>::clone() const {
  return new FuncUnary1_ColumnImpl<TI, TO>(
                Column(arg_), func_, nrows_, stype_);
}


template<typename TI, typename TO>
bool FuncUnary1_ColumnImpl<TI, TO>::get_element(size_t i, TO* out) const {
  TI x;
  bool xvalid = arg_.get_element(i, &x);
  if (!xvalid) return false;
  TO value = func_(x);
  *out = value;
  return !std::isnan(value);
}




//------------------------------------------------------------------------------
// FuncUnary2_ColumnImpl
//------------------------------------------------------------------------------

template<typename TI, typename TO>
FuncUnary2_ColumnImpl<TI, TO>::FuncUnary2_ColumnImpl(Column&& col, OP f)
  : FuncUnary2_ColumnImpl(std::move(col), f, col.nrows(), stype_from<TO>) {}


template<typename TI, typename TO>
FuncUnary2_ColumnImpl<TI, TO>::FuncUnary2_ColumnImpl(
    Column&& col, OP f, size_t nrows, SType stype
)
  : Virtual_ColumnImpl(nrows, stype),
    arg_(std::move(col)),
    func_(f)
{
  assert_compatible_type<TO>(stype);
}


template<typename TI, typename TO>
ColumnImpl* FuncUnary2_ColumnImpl<TI, TO>::clone() const {
  return new FuncUnary2_ColumnImpl<TI, TO>(
                Column(arg_), func_, nrows_, stype_);
}

template<typename TI, typename TO>
bool FuncUnary2_ColumnImpl<TI, TO>::get_element(size_t i, TO* out) const {
  TI x;
  bool xvalid = arg_.get_element(i, &x);
  return func_(x, xvalid, out);
}




}  // namespace dt
#endif
