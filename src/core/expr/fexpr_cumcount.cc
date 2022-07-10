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
#include "column/cumcount.h"
#include "column/latent.h"
#include "documentation.h"
#include "expr/fexpr_func.h"
#include "expr/eval_context.h"
#include "expr/workframe.h"
#include "python/xargs.h"
#include "stype.h"

namespace dt {
  namespace expr {

    class FExpr_CumCount : public FExpr_Func {
      private:
        bool arg_;

      public:
        FExpr_CumCount(bool arg)
            : arg_(arg) {}

        std::string repr() const override {
          std::string out = "cumcount";
          out += '(';
          out += "arg=";
          out += arg_? "False" : "True";
          out += ')';
          return out;
        }


        Workframe evaluate_n(EvalContext &ctx) const override {
          Workframe wf(ctx);

          if (ctx.has_groupby()) {
            wf.increase_grouping_mode(Grouping::GtoALL);
          }

          Groupby gby = ctx.get_groupby();
        
          
          size_t nrows = ctx.nrows();
          Column col = evaluate1(nrows, arg_, gby);
          wf.add_column(std::move(col), std::string(), wf.get_grouping_mode());
          return wf;
        }


        Column evaluate1(size_t n_rows, bool ascending, const Groupby &gby) const {
            return Column(new CUMCOUNT_ColumnImpl(n_rows, ascending, gby));
        }



    };


    static py::oobj pyfn_cumcount(const py::XArgs &args) {
      auto cumcount = args[0].to<bool>(true);
      return PyFExpr::make(new FExpr_CumCount(cumcount));
    }

    DECLARE_PYFN(&pyfn_cumcount)
        ->name("cumcount")
        //->docs(doc_dt_cumcount)
        ->arg_names({"reverse"})
        ->n_positional_args(1)
        ->n_required_args(1);

  }  // namespace dt::expr
}    // namespace dt
