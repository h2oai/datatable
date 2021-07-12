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

static const char* doc_day_of_week =
R"(day_of_week(date)
--
.. x-version-added:: 1.0.0

For a given date column compute the corresponding days of week.

Days of week are returned as integers from 1 to 7, where 1 represents
Monday, and 7 is Sunday. Thus, the return value of this function
matches the ISO standard.


Parameters
----------
date: FExpr[date32] | FExpr[time64]
    The date32 (or time64) column for which you need to calculate days of week.

return: FExpr[int32]
    An integer column, with values between 1 and 7 inclusive.


Examples
--------
>>> DT = dt.Frame([18000, 18600, 18700, 18800, None], stype='date32')
>>> DT[:, {"date": f[0], "day-of-week": dt.time.day_of_week(f[0])}]
   | date        day-of-week
   | date32            int32
-- + ----------  -----------
 0 | 2019-04-14            7
 1 | 2020-12-04            5
 2 | 2021-03-14            7
 3 | 2021-06-22            2
 4 | NA                   NA
[5 rows x 2 columns]
)";

static py::oobj pyfn_day_of_week(const py::XArgs& args) {
  auto arg = args[0].to_oobj();
  return PyFExpr::make(new FExpr_DayOfWeek(as_fexpr(arg)));
}

DECLARE_PYFN(&pyfn_day_of_week)
    ->name("day_of_week")
    ->docs(doc_day_of_week)
    ->arg_names({"date"})
    ->n_positional_args(1)
    ->n_required_args(1);




}}  // dt::expr
