//------------------------------------------------------------------------------
// Copyright 2022 H2O.ai
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
#include "column/const.h"
#include "column/cumminmax.h"
#include "column/latent.h"
#include "documentation.h"
#include "expr/fexpr_func.h"
#include "expr/eval_context.h"
#include "expr/workframe.h"
#include "python/xargs.h"
#include "stype.h"


namespace dt {
namespace expr {

template <bool MIN, bool REVERSE>
class FExpr_CumMinMax : public FExpr_Func {
  private:
    ptrExpr arg_;

  public:
    FExpr_CumMinMax(ptrExpr&& arg)
      : arg_(std::move(arg)) {}

    std::string repr() const override {
      std::string out = MIN? "cummin" : "cummax";
      out += '(';
      out += arg_->repr();
      out += ", reverse=";
      out += REVERSE? "True" : "False";
      out += ')';
      return out;
    }


    Workframe evaluate_n(EvalContext& ctx) const override {
      Workframe wf = arg_->evaluate_n(ctx);
      Groupby gby = ctx.get_groupby();

      if (!gby) {
        gby = Groupby::single_group(wf.nrows());
      } else {
        wf.increase_grouping_mode(Grouping::GtoALL);
      }

      for (size_t i = 0; i < wf.ncols(); ++i) {
        Column coli = wf.retrieve_column(i);
        auto typei = coli.type();
        if (!(typei.is_numeric_or_void() || typei.is_boolean() ||
              typei.is_temporal()))
        {
          throw TypeError()
            << "Invalid column of type `" << typei << "` in " << repr();
        }

        bool is_grouped = ctx.has_group_column(
                            wf.get_frame_id(i),
                            wf.get_column_id(i)
                          );
        if (!is_grouped) coli = evaluate1(std::move(coli), gby);
        wf.replace_column(i, std::move(coli));
      }
      return wf;
    }


    Column evaluate1(Column&& col, const Groupby& gby) const {
      SType stype = col.stype();
      switch (stype) {
        case SType::VOID:    return Column(new ConstNa_ColumnImpl(col.nrows()));
        case SType::BOOL:
        case SType::INT8:    return make<int8_t>(std::move(col), gby);
        case SType::INT16:   return make<int16_t>(std::move(col), gby);
        case SType::DATE32:
        case SType::INT32:   return make<int32_t>(std::move(col), gby);
        case SType::TIME64:
        case SType::INT64:   return make<int64_t>(std::move(col), gby);
        case SType::FLOAT32: return make<float>(std::move(col), gby);
        case SType::FLOAT64: return make<double>(std::move(col), gby);
        default: throw RuntimeError();
      }
    }


    template <typename T>
    Column make(Column&& col, const Groupby& gby) const {
      return Column(new Latent_ColumnImpl(
        new CumMinMax_ColumnImpl<T, MIN, REVERSE>(std::move(col), gby)
      ));
    }
};


static py::oobj pyfn_cummax(const py::XArgs& args) {
  auto cummax = args[0].to_oobj();
  auto reverse = args[1].to<bool>(false);
  if (reverse) {
    return PyFExpr::make(new FExpr_CumMinMax<false, true>(as_fexpr(cummax)));
  } else {
    return PyFExpr::make(new FExpr_CumMinMax<false, false>(as_fexpr(cummax)));
  }
}


static py::oobj pyfn_cummin(const py::XArgs& args) {
  auto cummin = args[0].to_oobj();
  auto reverse = args[1].to<bool>(false);
  if (reverse) {
    return PyFExpr::make(new FExpr_CumMinMax<true, true>(as_fexpr(cummin)));
  } else {
    return PyFExpr::make(new FExpr_CumMinMax<true, false>(as_fexpr(cummin)));
  }
}


DECLARE_PYFN(&pyfn_cummax)
    ->name("cummax")
    ->docs(doc_dt_cummax)
    ->arg_names({"cols", "reverse"})
    ->n_positional_args(1)
    ->n_positional_or_keyword_args(1)
    ->n_required_args(1);


DECLARE_PYFN(&pyfn_cummin)
    ->name("cummin")
    ->docs(doc_dt_cummin)
    ->arg_names({"cols", "reverse"})
    ->n_positional_args(1)
    ->n_positional_or_keyword_args(1)
    ->n_required_args(1);


}}  // dt::expr
