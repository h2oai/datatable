#include "column/const.h"
#include "documentation.h"
#include "expr/fexpr_func.h"
#include "expr/eval_context.h"
#include "expr/workframe.h"
#include "python/xargs.h"
#include "stype.h"
#include "column/nth.h"
namespace dt {
namespace expr {

template<bool SKIPNA>
class FExpr_Nth : public FExpr_Func {
  private:
    ptrExpr arg_;
    int32_t n_;

  public:
    FExpr_Nth(ptrExpr&& arg, py::oobj n)
      : arg_(std::move(arg))
       {n_ = n.to_int32_strict();}

    std::string repr() const override {
      std::string out = "nth";
      out += '(';
      out += arg_->repr();
      out += ", n=";
      out += std::to_string(n_);
      out += ", skipna=";
      out += SKIPNA? "True" : "False";
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
        Workframe outputs(ctx);
        for (size_t i = 0; i < wf.ncols(); ++i) {
          Column coli = wf.retrieve_column(i);
          coli = evaluate1(std::move(coli), n_, gby);
          outputs.add_column(std::move(coli), std::string(), Grouping::GtoONE);
        }

        return outputs;
    }     

    Column evaluate1(Column&& col, int32_t n, const Groupby& gby) const {
      SType stype = col.stype();
      switch (stype) {
        case SType::VOID:    return Column(new ConstNa_ColumnImpl(gby.size(), stype));
        case SType::BOOL:
        case SType::INT8:    return make<int8_t>(std::move(col), n, gby);
        case SType::INT16:   return make<int16_t>(std::move(col), n, gby);
        case SType::DATE32:
        case SType::INT32:   return make<int32_t>(std::move(col), n, gby);
        case SType::TIME64:
        case SType::INT64:   return make<int64_t>(std::move(col), n, gby);
        case SType::FLOAT32: return make<float>(std::move(col), n, gby);
        case SType::FLOAT64: return make<double>(std::move(col), n, gby);
        case SType::STR32:   return make<CString>(std::move(col), n, gby);
        case SType::STR64:   return make<CString>(std::move(col), n, gby);
        default: throw RuntimeError();
      }
    }

    template <typename T>
    Column make(Column&& col, int32_t n, const Groupby& gby) const {
      return Column(new NTH_ColumnImpl<T, SKIPNA>(std::move(col), n, gby));
    }
};


static py::oobj pyfn_nth(const py::XArgs& args) {
  auto arg = args[0].to_oobj();
  auto n = args[1].to_oobj();
  auto skipna = args[2].to<bool>(false);
  if (!n.is_int()) {
    throw TypeError() << "The argument for the `n` parameter "
                <<"in function datatable.nth() should be an integer, "
                <<"instead got "<<n.typeobj();
  }
  if (skipna) {
    return PyFExpr::make(new FExpr_Nth<true>(as_fexpr(arg), n));
  } else {
    return PyFExpr::make(new FExpr_Nth<false>(as_fexpr(arg), n));
  }
}


DECLARE_PYFN(&pyfn_nth)
    ->name("nth")
    //->docs(doc_dt_nth)
    ->arg_names({"cols", "n", "skipna"})
    ->n_positional_args(1)
    ->n_positional_or_keyword_args(2)
    ->n_required_args(2);

}}  // dt::expr

