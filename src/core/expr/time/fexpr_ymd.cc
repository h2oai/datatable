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
#include <algorithm>
#include "_dt.h"
#include "documentation.h"
#include "expr/eval_context.h"
#include "expr/fexpr_func.h"
#include "lib/hh/date.h"
#include "python/xargs.h"
#include "stype.h"
namespace dt {
namespace expr {



//------------------------------------------------------------------------------
// YMD_ColumnImpl
//------------------------------------------------------------------------------

class YMD_ColumnImpl : public Virtual_ColumnImpl {
  private:
    Column y_, m_, d_;

  public:
    YMD_ColumnImpl(Column&& y, Column&& m, Column&& d)
      : Virtual_ColumnImpl(y.nrows(), dt::SType::DATE32),
        y_(std::move(y)), m_(std::move(m)), d_(std::move(d))
    {
      xassert(y_.stype() == dt::SType::INT32);
      xassert(m_.stype() == dt::SType::INT32);
      xassert(d_.stype() == dt::SType::INT32);
    }

    ColumnImpl* clone() const override {
      return new YMD_ColumnImpl(Column(y_), Column(m_), Column(d_));
    }

    size_t n_children() const noexcept override {
      return 3;
    }

    const Column& child(size_t i) const override {
      xassert(i <= 2);
      return i == 0? y_ : i == 1? m_ : d_;
    }

    bool get_element(size_t i, int32_t* out) const override {
      int y, m, d;
      bool validY = y_.get_element(i, &y);
      bool validM = m_.get_element(i, &m);
      bool validD = d_.get_element(i, &d);
      if (validY && validM && validD &&
          (m >= 1 && m <= 12) &&
          (d >= 1 && d <= hh::last_day_of_month(y, m)))
      {
        *out = hh::days_from_civil(y, m, d);
        return true;
      }
      return false;
    }
};




//------------------------------------------------------------------------------
// FExpr_YMD
//------------------------------------------------------------------------------

class FExpr_YMD : public FExpr_Func {
  private:
    ptrExpr y_, m_, d_;

  public:
    FExpr_YMD(py::robj argY, py::robj argM, py::robj argD)
      : y_(as_fexpr(argY)),
        m_(as_fexpr(argM)),
        d_(as_fexpr(argD))
    {}

    std::string repr() const override {
      std::string out = "time.ymd(";
      out += y_->repr();
      out += ", ";
      out += m_->repr();
      out += ", ";
      out += d_->repr();
      out += ")";
      return out;
    }


    Workframe evaluate_n(EvalContext& ctx) const override {
      std::vector<Workframe> wfs;
      wfs.push_back(y_->evaluate_n(ctx));
      wfs.push_back(m_->evaluate_n(ctx));
      wfs.push_back(d_->evaluate_n(ctx));

      size_t ncolsY = wfs[0].ncols();
      size_t ncolsM = wfs[1].ncols();
      size_t ncolsD = wfs[2].ncols();
      size_t ncols = std::max(ncolsY, std::max(ncolsM, ncolsD));
      if (!((ncolsY == ncols || ncolsY == 1) &&
            (ncolsM == ncols || ncolsM == 1) &&
            (ncolsD == ncols || ncolsD == 1))) {
        throw InvalidOperationError() << "Incompatible numbers of columns for "
          "the year, month and day arguments of the ymd() function: " << ncolsY
          << ", " << ncolsM << ", and " << ncolsD;
      }
      if (ncolsY == 1) wfs[0].repeat_column(ncols);
      if (ncolsM == 1) wfs[1].repeat_column(ncols);
      if (ncolsD == 1) wfs[2].repeat_column(ncols);
      auto gmode = Workframe::sync_grouping_mode(wfs);

      Workframe result(ctx);
      for (size_t i = 0; i < ncols; ++i) {
        Column rescol = evaluate1(wfs[0].retrieve_column(i),
                                  wfs[1].retrieve_column(i),
                                  wfs[2].retrieve_column(i), i);
        result.add_column(std::move(rescol), std::string(), gmode);
      }
      return result;
    }

    static Column evaluate1(Column&& ycol, Column&& mcol, Column&& dcol,
                            size_t i)
    {
      if (!ycol.type().is_integer()) {
        throw TypeError() << "The year column at index " << i << " is of type "
            << ycol.type() << ", integer expected";
      }
      if (!mcol.type().is_integer()) {
        throw TypeError() << "The month column at index " << i << " is of type "
            << mcol.type() << ", integer expected";
      }
      if (!dcol.type().is_integer()) {
        throw TypeError() << "The day column at index " << i << " is of type "
            << dcol.type() << ", integer expected";
      }
      ycol.cast_inplace(dt::Type::int32());
      mcol.cast_inplace(dt::Type::int32());
      dcol.cast_inplace(dt::Type::int32());
      return Column(new YMD_ColumnImpl(
          std::move(ycol), std::move(mcol), std::move(dcol)));
    }
};




//------------------------------------------------------------------------------
// Python-facing `ymd()` function
//------------------------------------------------------------------------------

static py::oobj pyfn_ymd(const py::XArgs& args) {
  auto y = args[0].to_oobj();
  auto m = args[1].to_oobj();
  auto d = args[2].to_oobj();
  return PyFExpr::make(new FExpr_YMD(y, m, d));
}

DECLARE_PYFN(&pyfn_ymd)
    ->name("ymd")
    ->docs(doc_time_ymd)
    ->arg_names({"year", "month", "day"})
    ->n_positional_or_keyword_args(3)
    ->n_required_args(3);




}}  // dt::expr
