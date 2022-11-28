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
#include "column/sumprod.h"
#include "documentation.h"
#include "expr/fexpr_func.h"
#include "expr/eval_context.h"
#include "expr/workframe.h"
#include "python/xargs.h"
#include "stype.h"

namespace dt {
namespace expr {


template <bool SUM>
class FExpr_SumProd : public FExpr_Func {
  private:
    ptrExpr arg_;

  public:
    FExpr_SumProd(ptrExpr &&arg)
        : arg_(std::move(arg)) {}

    std::string repr() const override {
      std::string out = SUM? "sum" : "prod";
      out += '(';
      out += arg_->repr();
      out += ')';
      return out;
    }


    Workframe evaluate_n(EvalContext &ctx) const override {
      Workframe wf = arg_->evaluate_n(ctx);
      Groupby gby = ctx.get_groupby();
      if (!gby) {
        gby = Groupby::single_group(wf.nrows());
      }
      
      Workframe outputs(ctx);
      for (size_t i = 0; i < wf.ncols(); ++i) {
         Column coli = evaluate1(wf.retrieve_column(i), gby);
         outputs.add_column(std::move(coli), wf.retrieve_name(i), Grouping::GtoONE);
      }
      return outputs;
    }


    Column evaluate1(Column &&col, const Groupby &gby) const {
      SType stype = col.stype();
      switch (stype) {
        case SType::VOID:
          if (SUM) {
            return Column(new ConstInt_ColumnImpl(gby.size(), 0, SType::INT64));
          } else {
            return Column(new ConstInt_ColumnImpl(gby.size(), 1, SType::INT64));
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
       return Column(new SumProd_ColumnImpl<T, SUM>(std::move(col), gby));
    }
};


static py::oobj pyfn_sum(const py::XArgs &args) {
  auto sum = args[0].to_oobj();
    return PyFExpr::make(new FExpr_SumProd<true>(as_fexpr(sum)));
}


static py::oobj pyfn_prod(const py::XArgs &args) {
  auto prod = args[0].to_oobj();
    return PyFExpr::make(new FExpr_SumProd<false>(as_fexpr(prod)));
}


DECLARE_PYFN(&pyfn_sum)
    ->name("sum")
    ->docs(doc_dt_sum)
    ->arg_names({"cols"})
    ->n_positional_args(1)
    ->n_required_args(1);

DECLARE_PYFN(&pyfn_prod)
    ->name("prod")
    ->docs(doc_dt_prod)
    ->arg_names({"cols"})
    ->n_positional_args(1)
    ->n_required_args(1);


}}  // dt::expr
