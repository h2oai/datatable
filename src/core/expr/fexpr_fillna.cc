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
#include "column/fillna.h"
#include "column/latent.h"
#include "documentation.h"
#include "expr/fexpr_func.h"
#include "expr/eval_context.h"
#include "expr/workframe.h"
#include "python/xargs.h"
#include "stype.h"
#include <iostream>
#include "column/isna.h"
#include "sort.h"


namespace dt {
  namespace expr {

    class FExpr_FillNA : public FExpr_Func {
      private:
        ptrExpr arg_;
        bool reverse_;

      public:
        FExpr_FillNA(ptrExpr &&arg, bool reverse)
            : arg_(std::move(arg)),
              reverse_(reverse)
             {}

        std::string repr() const override {
          std::string out = "fillna";
          out += '(';
          out += arg_->repr();
          out += ", reverse=";
          out += reverse_? "True" : "False";
          out += ')';
          return out;
        }


        Workframe evaluate_n(EvalContext &ctx) const override {
          Workframe wf = arg_->evaluate_n(ctx);
          Workframe out(ctx);
          for (size_t i = 0; i < wf.ncols(); ++i) {
            Column coli = wf.retrieve_column(i);
            coli = Column(new Isna_ColumnImpl<int8_t>(std::move(coli)));
            RowIndex ri = RowIndex(std::move(coli));
            coli.apply_rowindex(ri.negate(coli.nrows())); // i get bools here
            out.add_column(std::move(coli), std::string(), out.get_grouping_mode());
          }
          return out;
        }

    };



    static py::oobj pyfn_fillna(const py::XArgs &args) {
      auto arg0 = args[0].to_oobj();
      auto reverse = args[1].to<bool>(false);


      return PyFExpr::make(new FExpr_FillNA(as_fexpr(arg0), reverse));
    }


    DECLARE_PYFN(&pyfn_fillna)
        ->name("fillna")
        //->docs(doc_dt_fillna)
        ->arg_names({"column", "reverse"})
        ->n_required_args(1)
        ->n_positional_args(1)
        ->n_keyword_args(1);

  }  // namespace dt::expr
}    // namespace dt
