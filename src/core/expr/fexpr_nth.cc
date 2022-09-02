#include "column/const.h"
#include "column/latent.h"
#include "documentation.h"
#include "expr/fexpr_func.h"
#include "expr/eval_context.h"
#include "expr/workframe.h"
#include "python/xargs.h"
#include "stype.h"
#include "parallel/api.h"
#include "rowindex.h"
#include <iostream>
namespace dt {
namespace expr {

class FExpr_Nth : public FExpr_Func {
  private:
    ptrExpr arg_;
    int32_t nth_arg_;

  public:
    FExpr_Nth(ptrExpr&& arg, py::oobj nth_arg)
      : arg_(std::move(arg))
       {nth_arg_ = nth_arg.to_int32_strict();}

    std::string repr() const override {
      std::string out = "nth";
      out += '(';
      out += arg_->repr();
      out += ", nth=";
      out += std::to_string(nth_arg_);
      out += ')';
      return out;
    }

    static RowIndex nth_rowindex(int32_t nth_arg, const Groupby& gby) {
      Buffer buf = Buffer::mem(gby.size() * sizeof(int32_t));
      int32_t* indices = static_cast<int32_t*>(buf.xptr());

      dt::parallel_for_dynamic(
        gby.size(),
        [&](size_t gi) {
          size_t i1, i2;
          gby.get_group(gi, &i1, &i2);
          nth_arg = nth_arg < 0 ? nth_arg + (i2 - i1) 
                                : nth_arg;
          indices[gi] = nth_arg >= 0 & nth_arg < i2 - i1 ? nth_arg + i1
                                          : RowIndex::NA<int32_t>;
            }
      );
      return RowIndex(std::move(buf), RowIndex::ARR32|RowIndex::SORTED);

    }
    
    Workframe evaluate_n(EvalContext &ctx) const override {
        Workframe wf = arg_->evaluate_n(ctx);
        Groupby gby = Groupby::single_group(wf.nrows());

        if (ctx.has_groupby()) {
            wf.increase_grouping_mode(Grouping::GtoALL);
            gby = ctx.get_groupby();
        }
        Workframe outputs(ctx);
        RowIndex ri = nth_rowindex(nth_arg_, gby);
        for (size_t i = 0; i < wf.ncols(); ++i) {
          Column coli = wf.retrieve_column(i);
          coli.apply_rowindex(ri);
          outputs.add_column(std::move(coli), std::string(), Grouping::GtoONE);
        }

        return outputs;
    }

    
    
};


static py::oobj pyfn_nth(const py::XArgs& args) {
  auto arg = args[0].to_oobj();
  auto nth_arg = args[1].to_oobj();
  if (!nth_arg.is_int()) {
    throw ValueError() << "The argument for the `nth` parameter "
                <<"in `nth()` function should be an integer, "
                <<"instead got "<<nth_arg.typeobj();
  }
  return PyFExpr::make(new FExpr_Nth(as_fexpr(arg), nth_arg));
}


DECLARE_PYFN(&pyfn_nth)
    ->name("nth")
    //->docs(doc_dt_nth)
    ->arg_names({"cols", "nth"})
    ->n_positional_args(2)
    ->n_required_args(2);

}}  // dt::expr

