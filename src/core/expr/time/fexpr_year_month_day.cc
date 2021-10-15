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
// YearMonthDay_ColumnImpl
//------------------------------------------------------------------------------

/**
  * We use a single virtual column to handle all three functions
  * year(), month() and day().
  *
  * <Kind>: 1 = Year, 2 = Month, 3 = Day
  */
template <int Kind>
class YearMonthDay_ColumnImpl : public Virtual_ColumnImpl {
  private:
    Column arg_;

  public:
    YearMonthDay_ColumnImpl(Column&& arg)
      : Virtual_ColumnImpl(arg.nrows(), dt::SType::INT32),
        arg_(std::move(arg))
    {
      xassert(arg_.stype() == dt::SType::DATE32);
    }

    ColumnImpl* clone() const override {
      return new YearMonthDay_ColumnImpl<Kind>(Column(arg_));
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
        auto ymd = hh::civil_from_days(value);
        if (Kind == 1) *out = ymd.year;
        if (Kind == 2) *out = ymd.month;
        if (Kind == 3) *out = ymd.day;
      }
      return isvalid;
    }
};




//------------------------------------------------------------------------------
// FExpr_YearMonthDay
//------------------------------------------------------------------------------

template <int Kind>
class FExpr_YearMonthDay : public FExpr_FuncUnary {
  public:
    using FExpr_FuncUnary::FExpr_FuncUnary;

    std::string name() const override {
      if (Kind == 1) return "time.year";
      if (Kind == 2) return "time.month";
      if (Kind == 3) return "time.day";
    }

    Column evaluate1(Column&& col) const override {
      if (col.stype() == dt::SType::VOID) {
        return Column::new_na_column(col.nrows(), dt::SType::VOID);
      }
      if (col.stype() == dt::SType::TIME64) {
        col.cast_inplace(dt::SType::DATE32);
      }
      if (col.stype() == dt::SType::DATE32) {
        return Column(new YearMonthDay_ColumnImpl<Kind>(std::move(col)));
      }
      throw TypeError() << "Function " << name() << "() requires a date32 or "
          "time64 column, instead received column of type " << col.type();
    }
};




//------------------------------------------------------------------------------
// Python-facing `year()`, `month()`, `day()` functions
//------------------------------------------------------------------------------

static py::oobj pyfn_year_month_day(const py::XArgs& args) {
  auto date_expr = as_fexpr(args[0].to_oobj());
  const int kind = args.get_info();
  if (kind == 1) return PyFExpr::make(new FExpr_YearMonthDay<1>(std::move(date_expr)));
  if (kind == 2) return PyFExpr::make(new FExpr_YearMonthDay<2>(std::move(date_expr)));
  if (kind == 3) return PyFExpr::make(new FExpr_YearMonthDay<3>(std::move(date_expr)));
  throw RuntimeError();
}

DECLARE_PYFN(&pyfn_year_month_day)
    ->name("year")
    ->docs(doc_time_year)
    ->arg_names({"date"})
    ->n_positional_args(1)
    ->n_required_args(1)
    ->add_info(1);

DECLARE_PYFN(&pyfn_year_month_day)
    ->name("month")
    ->docs(doc_time_month)
    ->arg_names({"date"})
    ->n_positional_args(1)
    ->n_required_args(1)
    ->add_info(2);

DECLARE_PYFN(&pyfn_year_month_day)
    ->name("day")
    ->docs(doc_time_day)
    ->arg_names({"date"})
    ->n_positional_args(1)
    ->n_required_args(1)
    ->add_info(3);




}}  // dt::expr
