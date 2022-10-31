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
#include "column/cumsumprod.h"
#include "column/latent.h"
#include "documentation.h"
#include "expr/fexpr_func.h"
#include "expr/eval_context.h"
#include "expr/workframe.h"
#include "python/xargs.h"
#include "stype.h"
namespace dt {
namespace expr {


template <bool SUM, bool REVERSE>
class FExpr_CumSumProd : public FExpr_Func {
  private:
    ptrExpr arg_;

  public:
    FExpr_CumSumProd(ptrExpr &&arg)
        : arg_(std::move(arg)) {}

    std::string repr() const override {
      std::string out = SUM? "cumsum" : "cumprod";
      out += '(';
      out += arg_->repr();
      out += ", reverse=";
      out += REVERSE? "True" : "False";
      out += ')';
      return out;
    }


    Workframe evaluate_n(EvalContext &ctx) const override {
      Workframe wf = arg_->evaluate_n(ctx);
      Groupby gby = ctx.get_groupby();
      if (!gby) {
        gby = Groupby::single_group(wf.nrows());
      } else {
        wf.increase_grouping_mode(Grouping::GtoALL);
      }

      for (size_t i = 0; i < wf.ncols(); ++i) {
        Column coli = evaluate1(wf.retrieve_column(i), gby);
        wf.replace_column(i, std::move(coli));
      }
      return wf;
    }


    Column evaluate1(Column &&col, const Groupby &gby) const {
      SType stype = col.stype();
      switch (stype) {
        case SType::VOID:
          if (SUM) {
            return Column(new ConstInt_ColumnImpl(col.nrows(), 0, SType::INT64));
          } else {
            return Column(new ConstInt_ColumnImpl(col.nrows(), 1, SType::INT64));
          }
        case SType::BOOL:
        case SType::INT8:
        case SType::INT16:
        case SType::INT32:
        case SType::INT64:
          return make<int64_t>(std::move(col), SType::INT64, gby);
        case SType::FLOAT32:
          return make<float>(std::move(col), SType::FLOAT32, gby);
        case SType::FLOAT64:
          return make<double>(std::move(col), SType::FLOAT64, gby);
        default:
          throw TypeError()
            << "Invalid column of type `" << stype << "` in " << repr();
      }
    }


    template <typename T>
    Column make(Column &&col, SType stype, const Groupby &gby) const {
       col.cast_inplace(stype);
       return Column(new Latent_ColumnImpl(
         new CumSumProd_ColumnImpl<T, SUM, REVERSE>(std::move(col), gby)
       ));
    }
};


static py::oobj pyfn_cumsum(const py::XArgs &args) {
  auto cumsum = args[0].to_oobj();
  auto reverse = args[1].to<bool>(false);
  if (reverse) {
    return PyFExpr::make(new FExpr_CumSumProd<true, true>(as_fexpr(cumsum)));
  } else {
    return PyFExpr::make(new FExpr_CumSumProd<true, false>(as_fexpr(cumsum)));
  }
}

static py::oobj pyfn_cumprod(const py::XArgs &args) {
  auto cumprod = args[0].to_oobj();
  auto reverse = args[1].to<bool>(false);
  if (reverse) {
    return PyFExpr::make(new FExpr_CumSumProd<false, true>(as_fexpr(cumprod)));
  } else {
    return PyFExpr::make(new FExpr_CumSumProd<false, false>(as_fexpr(cumprod)));
  }
}

DECLARE_PYFN(&pyfn_cumsum)
    ->name("cumsum")
    ->docs(doc_dt_cumsum)
    ->arg_names({"cols", "reverse"})
    ->n_positional_args(1)
    ->n_positional_or_keyword_args(1)
    ->n_required_args(1);

DECLARE_PYFN(&pyfn_cumprod)
    ->name("cumprod")
    ->docs(doc_dt_cumprod)
    ->arg_names({"cols", "reverse"})
    ->n_positional_args(1)
    ->n_positional_or_keyword_args(1)
    ->n_required_args(1);


}}  // dt::expr
