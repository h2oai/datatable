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
#include "documentation.h"
#include "expr/fexpr_func.h"
#include "expr/eval_context.h"
#include "python/xargs.h"
#include "parallel/api.h"
namespace dt {
namespace expr {


class FExpr_FillNA : public FExpr_Func {
  private:
    ptrExpr arg_;
    bool reverse_;
    size_t : 56;

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
    static RowIndex fill_rowindex(Column& col, const Groupby& gby) {
      Buffer buf = Buffer::mem(static_cast<size_t>(col.nrows()) * sizeof(int32_t));
      auto indices = static_cast<int32_t*>(buf.xptr());

      dt::parallel_for_dynamic(
        gby.size(),
        [&](size_t gi) {
          size_t i1, i2;
          gby.get_group(gi, &i1, &i2);
          size_t fill_id = REVERSE? i2 - 1 : i1;

          if (REVERSE) {
            for (size_t i = i2; i-- > i1;) {
              size_t is_valid = col.get_element_isvalid(i);
              fill_id = is_valid? i : fill_id;
              indices[i] = static_cast<int32_t>(fill_id);
            }
          } else {
            for (size_t i = i1; i < i2; ++i) {
              size_t is_valid = col.get_element_isvalid(i);
              fill_id = is_valid? i : fill_id;
              indices[i] = static_cast<int32_t>(fill_id);
            }
          }

        }
      );

      return RowIndex(std::move(buf), RowIndex::ARR32|RowIndex::SORTED);
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
        bool is_grouped = ctx.has_group_column(
                            wf.get_frame_id(i),
                            wf.get_column_id(i)
                          );
        if (is_grouped) continue;

        Column coli = wf.retrieve_column(i);
        auto stats = coli.get_stats_if_exist();
        bool na_stats_exists = stats && stats->is_computed(Stat::NaCount);
        bool has_nas = na_stats_exists? stats->nacount()
                                      : true;

        if (has_nas) {
          RowIndex ri = reverse_? fill_rowindex<true>(coli, gby)
                                : fill_rowindex<false>(coli, gby);
          coli.apply_rowindex(ri);
        }

        wf.replace_column(i, std::move(coli));
      }

      return wf;
    }

};


static py::oobj pyfn_fillna(const py::XArgs &args) {
  auto column = args[0].to_oobj();
  auto reverse = args[1].to<bool>(false);
  return PyFExpr::make(new FExpr_FillNA(as_fexpr(column), reverse));
}


DECLARE_PYFN(&pyfn_fillna)
    ->name("fillna")
    ->docs(doc_dt_fillna)
    ->arg_names({"column", "reverse"})
    ->n_required_args(1)
    ->n_positional_args(1)
    ->n_positional_or_keyword_args(1);


}}  // dt::expr
