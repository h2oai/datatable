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
#ifndef dt_EXPR_FUNARY_UMAKER_IMPL_h
#define dt_EXPR_FUNARY_UMAKER_IMPL_h
#include "column/const.h"
#include "column/func_unary.h"
#include "expr/funary/umaker.h"
#include "utils/assert.h"
#include "column.h"
namespace dt {
namespace expr {



/**
  * Trivial "umaker" class that always returns an NA column.
  */
class umaker_nacol : public umaker {
  public:
    static umaker_ptr make() { return umaker_ptr(new umaker_nacol()); }

    Column compute(Column&& col) const override {
      if (col.stype() == SType::VOID) return std::move(col);
      return Column::new_na_column(col.nrows());
    }
};


/**
  * "umaker" class that always returns a constant column.
  */
class umaker_const : public umaker {
  private:
    Column res_;

  public:
    umaker_const(Column&& res) : res_(std::move(res)) {}

    Column compute(Column&& col) const override {
      Column out = res_;
      out.repeat(col.nrows());
      return out;
    }
};



/**
  * Simple "umaker" class that returns the column unchanged.
  */
class umaker_copy : public umaker {
  public:
    Column compute(Column&& col) const override {
      return std::move(col);
    }
};



/**
  * "umaker" class that casts its column into the given stype.
  */
class umaker_cast : public umaker {
  private:
    SType outtype_;
    size_t : 56;

  public:
    umaker_cast(SType out) : outtype_(out) {}

    Column compute(Column&& col) const override {
      col.cast_inplace(outtype_);
      return std::move(col);
    }
};




/**
  * "umaker" class which optionally upcasts its argument into
  * `uptype_`, and then creates a `FuncUnary1_ColumnImpl` column
  * (see "column/func_unary.h").
  *
  * Basically, this class is used to wrap binary operations with
  * trivial handling of NAs: if either of the arguments is NA then
  * the result is NA, if neither argument is NA then the result is
  * not NA either (except when TR is floating-point, in which case
  * it is allowed for non-NA arguments to produce NA result).
  */
template <typename TX, typename TR>
class umaker1 : public umaker
{
  using func_t = TR(*)(typename _ref<TX>::t);
  private:
    func_t func_;
    SType uptype_;
    SType outtype_;
    size_t : 48;

  public:
    umaker1(func_t f, SType up, SType out)
      : func_(f), uptype_(up), outtype_(out)
    {
      if (up != SType::VOID) assert_compatible_type<TX>(up);
      assert_compatible_type<TR>(out);
    }

    static umaker_ptr make(func_t f, SType up, SType out) {
      return umaker_ptr(new umaker1(f, up, out));
    }

    Column compute(Column&& col) const override {
      if (uptype_ != SType::VOID) col.cast_inplace(uptype_);
      size_t nrows = col.nrows();
      return Column(new FuncUnary1_ColumnImpl<TX, TR>(
                        std::move(col), func_, nrows, outtype_
                    ));
    }
};




/**
  * "umaker" class which optionally upcasts its argument into
  * `uptype_`, and then creates a `FuncUnary2_ColumnImpl` column
  * (see "column/func_unary.h").
  *
  * The primary difference with the previous class is the handling
  * of NAs: this class wraps a function which explicitly deals with
  * NAs both in the input and in the output:
  *
  *     (TX x, bool xvalid, TR* out) -> bool
  *
  */
template <typename TX, typename TR>
class umaker2 : public umaker
{
  using func_t = bool(*)(typename _ref<TX>::t, bool, TR*);
  private:
    func_t func_;
    SType uptype_;
    SType outtype_;
    size_t : 48;

  public:
    umaker2(func_t f, SType up, SType out)
      : func_(f), uptype_(up), outtype_(out)
    {
      if (up != SType::VOID) assert_compatible_type<TX>(up);
      assert_compatible_type<TR>(out);
    }

    static umaker_ptr make(func_t f, SType up, SType out) {
      return umaker_ptr(new umaker2(f, up, out));
    }

    Column compute(Column&& col) const override {
      if (uptype_ != SType::VOID) col.cast_inplace(uptype_);
      size_t nrows = col.nrows();
      return Column(new FuncUnary2_ColumnImpl<TX, TR>(
                        std::move(col), func_, nrows, outtype_
                    ));
    }
};




}}  // namespace dt::expr
#endif
