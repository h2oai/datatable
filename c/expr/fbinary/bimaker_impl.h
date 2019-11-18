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



template <typename T1, typename T2, typename TO>
class bimaker1 : public bimaker
{
  using func_t = TO(*)(typename _ref<T1>::t, typename _ref<T2>::t);
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
      return Column(new FuncBinary1_ColumnImpl<T1, T2, TO>(
                        std::move(col1), std::move(col2),
                        func_, nrows, outtype_
                    ));
    }
};


template <typename T1, typename T2, typename TO>
class bimaker2 : public bimaker
{
  using R1 = typename _ref<T1>::t;
  using R2 = typename _ref<T2>::t;
  using func_t = bool(*)(R1, bool, R2, bool, TO*);
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
      return Column(new FuncBinary2_ColumnImpl<T1, T2, TO>(
                        std::move(col1), std::move(col2),
                        func_, nrows, outtype_
                    ));
    }
};




}}  // namespace dt::expr
#endif
