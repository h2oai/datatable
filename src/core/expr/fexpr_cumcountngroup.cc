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
#include "column/cumcountngroup.h"
#include "column/latent.h"
#include "column/range.h"
#include "documentation.h"
#include "expr/fexpr_func.h"
#include "expr/eval_context.h"
#include "expr/workframe.h"
#include "python/xargs.h"
#include "stype.h"
namespace dt {
namespace expr {


//------------------------------------------------------------------------------
// FExpr_CumcountNgroup
//------------------------------------------------------------------------------

template<bool CUMCOUNT, bool REVERSE>
class FExpr_CumcountNgroup : public FExpr_Func {
  public:
    FExpr_CumcountNgroup(){}

    std::string repr() const override {
      std::string out = CUMCOUNT? "cumcount" : "ngroup";
      out += '(';
      out += "reverse=";
      out += REVERSE? "True" : "False";
      out += ')';
      return out;
    }


    Workframe evaluate_n(EvalContext &ctx) const override {
      Workframe wf(ctx);
      Column col;

      if (ctx.has_groupby()) {
        wf.increase_grouping_mode(Grouping::GtoALL);
        const Groupby& gby = ctx.get_groupby();
        col = Column(new Latent_ColumnImpl(
                new CumcountNgroup_ColumnImpl<CUMCOUNT, REVERSE>(gby)
              ));
      } else {
        if (CUMCOUNT) {
          auto nrows = static_cast<int64_t>(ctx.nrows());
          auto impl = REVERSE? new Range_ColumnImpl(nrows - 1, -1, -1, Type::int64())
                             : new Range_ColumnImpl(0, nrows, 1, Type::int64());
          col = Column(std::move(impl));
        } else {
          col = Column(new ConstInt_ColumnImpl(ctx.nrows(), 0, SType::INT64));
        }
      }

      wf.add_column(std::move(col), std::string(), Grouping::GtoALL);
      return wf;
    }

};


//------------------------------------------------------------------------------
// Python-facing `cumcount()` and `ngroup()` functions
//------------------------------------------------------------------------------

static py::oobj pyfn_cumcount(const py::XArgs &args) {
  auto reverse = args[0].to<bool>(false);
  if (reverse) {
    return PyFExpr::make(new FExpr_CumcountNgroup<true, true>());
  } else {
    return PyFExpr::make(new FExpr_CumcountNgroup<true, false>());
  }
}

static py::oobj pyfn_ngroup(const py::XArgs &args) {
  auto reverse = args[0].to<bool>(false);
  if (reverse) {
    return PyFExpr::make(new FExpr_CumcountNgroup<false, true>());
  } else {
    return PyFExpr::make(new FExpr_CumcountNgroup<false, false>());
  }
}


DECLARE_PYFN(&pyfn_cumcount)
    ->name("cumcount")
    ->docs(doc_dt_cumcount)
    ->n_positional_or_keyword_args(1)
    ->arg_names({"reverse"});

DECLARE_PYFN(&pyfn_ngroup)
    ->name("ngroup")
    ->docs(doc_dt_ngroup)
    ->n_positional_or_keyword_args(1)
    ->arg_names({"reverse"});


}}  // namespace dt::expr
