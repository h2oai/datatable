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
#include "documentation.h"
#include "expr/fexpr_func.h"
#include "expr/eval_context.h"
#include "expr/workframe.h"
#include "python/xargs.h"
#include "stype.h"

namespace dt {
  namespace expr {
    template<bool CUMCOUNT>
    class FExpr_CumCount : public FExpr_Func {
      private:
        bool arg_;

      public:
        FExpr_CumCount(bool arg)
            : arg_(arg) {}

        std::string repr() const override {
          std::string out = CUMCOUNT?"cumcount":"ngroup";
          out += '(';
          out += "arg=";
          out += arg_? "True" : "False";
          out += ')';
          return out;
        }


        Workframe evaluate_n(EvalContext &ctx) const override {
          Workframe wf(ctx);
          size_t nrows = ctx.nrows();
          Groupby gby = Groupby::single_group(nrows);

          if (ctx.has_groupby()) {
            wf.increase_grouping_mode(Grouping::GtoALL);
            gby = ctx.get_groupby();
          }     
                    
          Column col = evaluate1(nrows, arg_, gby);
          wf.add_column(std::move(col), std::string(), wf.get_grouping_mode());
          return wf;
        }

        Column evaluate1(size_t n_rows, bool reverse, const Groupby &gby) const {
            return Column(new CUMCOUNTNGROUP_ColumnImpl<CUMCOUNT>(n_rows, reverse, gby));
        }

    };


    static py::oobj pyfn_cumcount(const py::XArgs &args) {
      auto cumcount = args[0].to<bool>(false);
      return PyFExpr::make(new FExpr_CumCount<true>(cumcount));
    }

    static py::oobj pyfn_ngroup(const py::XArgs &args) {
      auto ngroup = args[0].to<bool>(false);
      return PyFExpr::make(new FExpr_CumCount<false>(ngroup));
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

  }  // namespace dt::expr
}    // namespace dt
