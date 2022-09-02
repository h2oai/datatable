#include "column/const.h"
#include "column/latent.h"
#include "documentation.h"
#include "expr/fexpr_func.h"
#include "expr/eval_context.h"
#include "expr/workframe.h"
#include "python/xargs.h"
#include "stype.h"

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
    
    Workframe evaluate_n(EvalContext &ctx) const override {
        Workframe wf = arg_->evaluate_n(ctx);
        Groupby gby = Groupby::single_group(wf.nrows());

        if (ctx.has_groupby()) {
            wf.increase_grouping_mode(Grouping::GtoALL);
            gby = ctx.get_groupby();
        }

        return wf;
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

