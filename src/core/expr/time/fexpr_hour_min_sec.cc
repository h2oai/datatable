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
#include "python/xargs.h"
#include "stype.h"
namespace dt {
namespace expr {



//------------------------------------------------------------------------------
// HourMinSec_ColumnImpl
//------------------------------------------------------------------------------

/**
  * We use a single virtual column to handle all four functions
  * hour(), minute(), second(), nanosecond().
  *
  * <Kind>: 1 = Hour, 2 = Minute, 3 = Second, 4 = Nanosecond
  */
template <int Kind>
class HourMinSec_ColumnImpl : public Virtual_ColumnImpl {
  private:
    Column arg_;

  public:
    HourMinSec_ColumnImpl(Column&& arg)
      : Virtual_ColumnImpl(arg.nrows(), dt::SType::INT32),
        arg_(std::move(arg))
    {
      xassert(arg_.stype() == dt::SType::TIME64);
    }

    ColumnImpl* clone() const override {
      return new HourMinSec_ColumnImpl<Kind>(Column(arg_));
    }

    size_t n_children() const noexcept override {
      return 1;
    }

    const Column& child(size_t i) const override {
      xassert(i == 0); (void)i;
      return arg_;
    }

    bool get_element(size_t i, int32_t* out) const override {
      constexpr int64_t DAY = 24ll * 3600ll * 1000000000ll;
      constexpr int64_t SCALE = (Kind == 1)? 3600ll * 1000000000ll :
                                (Kind == 2)? 60ll * 1000000000ll :
                                (Kind == 3)? 1000000000ll : 1;
      constexpr int64_t MODULO = (Kind == 1)? 24 :
                                 (Kind == 2)? 60 :
                                 (Kind == 3)? 60 : 1000000000ll;

      int64_t value;
      bool isvalid = arg_.get_element(i, &value);
      if (isvalid) {
        if (value < 0) {
          // This is only because C uses truncate-division instead of
          // floor-division for negative numbers.
          value = (value % DAY) + DAY;
        }
        *out = static_cast<int32_t>((value / SCALE) % MODULO);
      }
      return isvalid;
    }
};




//------------------------------------------------------------------------------
// FExpr_HourMinSec
//------------------------------------------------------------------------------

template <int Kind>
class FExpr_HourMinSec : public FExpr_FuncUnary {
  public:
    using FExpr_FuncUnary::FExpr_FuncUnary;

    std::string name() const override {
      if (Kind == 1) return "time.hour";
      if (Kind == 2) return "time.minute";
      if (Kind == 3) return "time.second";
      if (Kind == 4) return "time.nanosecond";
    }

    Column evaluate1(Column&& col) const override {
      if (col.stype() == dt::SType::VOID) {
        return Column::new_na_column(col.nrows(), dt::SType::VOID);
      }
      if (col.stype() == dt::SType::TIME64) {
        return Column(new HourMinSec_ColumnImpl<Kind>(std::move(col)));
      }
      throw TypeError() << "Function " << name() << "() requires a time64 "
          "column, instead received column of type " << col.type();
    }
};




//------------------------------------------------------------------------------
// Python-facing `hour()`, `minute()`, `second()`, and `nanosecond()` functions
//------------------------------------------------------------------------------

static py::oobj pyfn_hour_min_sec(const py::XArgs& args) {
  auto time_expr = as_fexpr(args[0].to_oobj());
  const int kind = args.get_info();
  if (kind == 1) return PyFExpr::make(new FExpr_HourMinSec<1>(std::move(time_expr)));
  if (kind == 2) return PyFExpr::make(new FExpr_HourMinSec<2>(std::move(time_expr)));
  if (kind == 3) return PyFExpr::make(new FExpr_HourMinSec<3>(std::move(time_expr)));
  if (kind == 4) return PyFExpr::make(new FExpr_HourMinSec<4>(std::move(time_expr)));
  throw RuntimeError();
}

DECLARE_PYFN(&pyfn_hour_min_sec)
    ->name("hour")
    ->docs(doc_time_hour)
    ->arg_names({"time"})
    ->n_positional_args(1)
    ->n_required_args(1)
    ->add_info(1);

DECLARE_PYFN(&pyfn_hour_min_sec)
    ->name("minute")
    ->docs(doc_time_minute)
    ->arg_names({"time"})
    ->n_positional_args(1)
    ->n_required_args(1)
    ->add_info(2);

DECLARE_PYFN(&pyfn_hour_min_sec)
    ->name("second")
    ->docs(doc_time_second)
    ->arg_names({"time"})
    ->n_positional_args(1)
    ->n_required_args(1)
    ->add_info(3);

DECLARE_PYFN(&pyfn_hour_min_sec)
    ->name("nanosecond")
    ->docs(doc_time_nanosecond)
    ->arg_names({"time"})
    ->n_positional_args(1)
    ->n_required_args(1)
    ->add_info(4);




}}  // dt::expr
