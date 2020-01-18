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
#ifndef dt_EXPR_FBINARY_BIMAKER_IMPL_h
#define dt_EXPR_FBINARY_BIMAKER_IMPL_h
#include "column/func_binary.h"
#include "expr/fbinary/bimaker.h"
#include "column.h"
namespace dt {
namespace expr {



/**
  * Trivial "bimaker" class that always returns an NA column.
  */
class bimaker_nacol : public bimaker {
  public:
    static bimaker_ptr make() { return bimaker_ptr(new bimaker_nacol()); }

    Column compute(Column&& col1, Column&& col2) const override {
      if (col1.stype() == SType::VOID) return std::move(col1);
      if (col2.stype() == SType::VOID) return std::move(col2);
      return Column::new_na_column(col1.nrows());
    }
};




/**
  * "bimaker" class which optionally upcasts its arguments into
  * `uptype1_` and `uptype2_`, and then creates a
  * `FuncBinary1_ColumnImpl` column (see "column/func_binary.h").
  *
  * Basically, this class is used to wrap binary operations with
  * trivial handling of NAs: if either of the arguments is NA then
  * the result is NA, if neither argument is NA then the result is
  * not NA either (except when TR is floating-point, in which case
  * it is allowed for non-NA arguments to produce NA result).
  */
template <typename TX, typename TY, typename TR>
class bimaker1 : public bimaker
{
  using func_t = TR(*)(typename _ref<TX>::t, typename _ref<TY>::t);
  private:
    func_t func_;
    SType uptype1_;
    SType uptype2_;
    SType outtype_;
    size_t : 40;

  public:
    bimaker1(func_t f, SType up1, SType up2, SType out)
      : func_(f), uptype1_(up1), uptype2_(up2), outtype_(out) {}

    static bimaker_ptr make(func_t f, SType up1, SType up2, SType out) {
      return bimaker_ptr(new bimaker1(f, up1, up2, out));
    }

    Column compute(Column&& col1, Column&& col2) const override {
      if (uptype1_ != SType::VOID) col1.cast_inplace(uptype1_);
      if (uptype2_ != SType::VOID) col2.cast_inplace(uptype2_);
      size_t nrows = col1.nrows();
      return Column(new FuncBinary1_ColumnImpl<TX, TY, TR>(
                        std::move(col1), std::move(col2),
                        func_, nrows, outtype_
                    ));
    }
};




/**
  * "bimaker" class which optionally upcasts its arguments into
  * `uptype1_` and `uptype2_`, and then creates a
  * `FuncBinary2_ColumnImpl` column (see "column/func_binary.h").
  *
  * The primary difference with the previous class is the handling
  * of NAs: this class wraps a function which explicitly deals with
  * NAs both in the inputs and in the output:
  *
  *     (TX x, bool xvalid, TY y, bool yvalid, TR* out) -> bool
  *
  */
template <typename TX, typename TY, typename TR>
class bimaker2 : public bimaker
{
  using func_t = bool(*)(typename _ref<TX>::t, bool,
                         typename _ref<TY>::t, bool, TR*);
  private:
    func_t func_;
    SType uptype1_;
    SType uptype2_;
    SType outtype_;
    size_t : 40;

  public:
    bimaker2(func_t f, SType up1, SType up2, SType out)
      : func_(f), uptype1_(up1), uptype2_(up2), outtype_(out) {}

    static bimaker_ptr make(func_t f, SType up1, SType up2, SType out) {
      return bimaker_ptr(new bimaker2(f, up1, up2, out));
    }

    Column compute(Column&& col1, Column&& col2) const override {
      if (uptype1_ != SType::VOID) col1.cast_inplace(uptype1_);
      if (uptype2_ != SType::VOID) col2.cast_inplace(uptype2_);
      size_t nrows = col1.nrows();
      return Column(new FuncBinary2_ColumnImpl<TX, TY, TR>(
                        std::move(col1), std::move(col2),
                        func_, nrows, outtype_
                    ));
    }
};




}}  // namespace dt::expr
#endif
