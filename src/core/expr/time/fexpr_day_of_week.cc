//------------------------------------------------------------------------------
// Copyright 2021 H2O.ai
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
#include <type_traits>
#include "_dt.h"
#include "documentation.h"
#include "expr/eval_context.h"
#include "expr/fexpr_func_unary.h"
#include "lib/hh/date.h"
#include "python/xargs.h"
#include "stype.h"
namespace dt {
namespace expr {



//------------------------------------------------------------------------------
// DayOfWeek_ColumnImpl
//------------------------------------------------------------------------------

class DayOfWeek_ColumnImpl : public Virtual_ColumnImpl {
  private:
    Column arg_;

  public:
    DayOfWeek_ColumnImpl(Column&& arg)
      : Virtual_ColumnImpl(arg.nrows(), dt::SType::INT32),
        arg_(std::move(arg))
    {
      xassert(arg_.stype() == dt::SType::DATE32);
    }

    ColumnImpl* clone() const override {
      return new DayOfWeek_ColumnImpl(Column(arg_));
    }

    size_t n_children() const noexcept override {
      return 1;
    }

    const Column& child(size_t i) const override {
      xassert(i == 0); (void)i;
      return arg_;
    }

    bool get_element(size_t i, int32_t* out) const override {
      int32_t value;
      bool isvalid = arg_.get_element(i, &value);
      if (isvalid) {
        *out = hh::iso_weekday_from_days(value);
      }
      return isvalid;
    }
};




//------------------------------------------------------------------------------
// FExpr_DayOfWeek
//------------------------------------------------------------------------------

class FExpr_DayOfWeek : public FExpr_FuncUnary {
  public:
    using FExpr_FuncUnary::FExpr_FuncUnary;

    std::string name() const override {
      return "time.day_of_week";
    }

    Column evaluate1(Column&& col) const override {
      if (col.stype() == dt::SType::VOID) {
        return Column::new_na_column(col.nrows(), dt::SType::VOID);
      }
      if (col.stype() == dt::SType::TIME64) {
        col.cast_inplace(dt::SType::DATE32);
      }
      if (col.stype() == dt::SType::DATE32) {
        return Column(
            new DayOfWeek_ColumnImpl(std::move(col)));
      }
      throw TypeError() << "Function " << name() << "() requires a date32 or "
          "time64 column, instead received column of type " << col.type();
    }
};




//------------------------------------------------------------------------------
// Python-facing `day_of_week()` function
//------------------------------------------------------------------------------

static py::oobj pyfn_day_of_week(const py::XArgs& args) {
  auto arg = args[0].to_oobj();
  return PyFExpr::make(new FExpr_DayOfWeek(as_fexpr(arg)));
}

DECLARE_PYFN(&pyfn_day_of_week)
    ->name("day_of_week")
    ->docs(doc_time_day_of_week)
    ->arg_names({"date"})
    ->n_positional_args(1)
    ->n_required_args(1);




}}  // dt::expr
