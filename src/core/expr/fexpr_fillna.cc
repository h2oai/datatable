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
//#include "column/fillna.h"
#include "column/latent.h"
#include "documentation.h"
#include "expr/fexpr_func.h"
#include "expr/eval_context.h"
#include "expr/workframe.h"
#include "python/xargs.h"
#include "parallel/api.h"
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

        template <bool REVERSE>
        static RowIndex fill_rowindex(const Groupby& groupby, Column& col) {
          Buffer buf_indices = Buffer::mem(static_cast<size_t>(col.nrows()) * sizeof(int32_t));
          int32_t* indices = static_cast<int32_t*>(buf_indices.xptr());
          auto group_offsets = groupby.offsets_r();
          
          dt::parallel_for_dynamic(
            groupby.size(),
            [&](size_t i){
              int32_t i0 = group_offsets[i];
              int32_t i1 = group_offsets[i+1];
              bool is_valid;
              int32_t prev = 0; 
              if (REVERSE) {
                while (i1 > i0) {
                i1--;
                is_valid = col.get_element_isvalid(i1);
                prev = is_valid?i1:prev;
                indices[i1] = prev;
              }} else {
                while (i0 < i1) {
                is_valid = col.get_element_isvalid(i0);
                prev = is_valid?i0:prev;
                indices[i0] = prev;
                i0++;
              }}
              }
          );
          return RowIndex(std::move(buf_indices), RowIndex::ARR32|RowIndex::SORTED);

        }


        Workframe evaluate_n(EvalContext &ctx) const override {
          Workframe wf = arg_->evaluate_n(ctx);
          if (wf.nrows() == 0){
            return wf;
          }
          Groupby gby = Groupby::single_group(wf.nrows());
          if (ctx.has_groupby()) {
            wf.increase_grouping_mode(Grouping::GtoALL);
            gby = ctx.get_groupby();
          }

          for (size_t i = 0; i < wf.ncols(); ++i) {
            Column coli = wf.retrieve_column(i);
            if (coli.na_count() > 0){     
              RowIndex ri = reverse_? fill_rowindex<true>(gby, coli) 
                                    : fill_rowindex<false>(gby, coli);
              coli.apply_rowindex(ri);
              }
            wf.replace_column(i, std::move(coli));
          }
          return wf;
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
