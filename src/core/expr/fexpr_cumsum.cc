#include "column/latent.h"
#include "column/virtual.h"
#include "documentation.h"
#include "expr/fexpr_func.h"
#include "expr/eval_context.h"
#include "expr/workframe.h"
#include "python/xargs.h"
#include "stype.h"
namespace dt {
namespace expr {


template <typename T>
class Column_cumsum : public Virtual_ColumnImpl {
  private:
    Column col_;

  public:
    Column_cumsum(Column&& col)
      : Virtual_ColumnImpl(col.nrows(), col.stype()),
        col_(std::move(col))
    {
      xassert(col_.can_be_read_as<T>());
    }

    ColumnImpl* clone() const override {
      return new Column_cumsum(Column(col_));
    }

    size_t n_children() const noexcept override { return 1; }
    const Column& child(size_t i) const override { xassert(i == 0);  (void)i;
      return col_; }

    bool get_element(size_t i, T* out) const override {
      xassert(i < col_.nrows());
      T cumsum = 0;
      bool cumsum_valid = false;

      for (size_t j = 0; j <= i; ++j) {
        T val;
        bool val_valid = col_.get_element(j, &val);
        if (val_valid) {
          cumsum_valid = true;
          cumsum += val;
        }
      }
      *out = cumsum;
      return cumsum_valid;
    }

};


class FExpr_cumsum : public FExpr_Func {
  private:
    ptrExpr cumsum_;

  public:
    FExpr_cumsum(ptrExpr&& cumsum)
      : cumsum_(std::move(cumsum)) {}

    std::string repr() const override{
  std::string out = "cumsum(";
  out += cumsum_->repr();
  out += ')';
  return out;
    }

    Workframe evaluate_n(EvalContext& ctx) const override{
      Workframe cumsum_wf = cumsum_->evaluate_n(ctx);

      auto gmode = cumsum_wf.get_grouping_mode();
      Workframe outputs(ctx);
      for (size_t i = 0; i < cumsum_wf.ncols(); ++i) {
        Column rescol = evaluate1(cumsum_wf.retrieve_column(i));
        outputs.add_column(std::move(rescol), std::string(), gmode);
      }
      return outputs;
    }

    Column evaluate1(Column&& cumsum) const {
      SType stype0 = cumsum.stype();
      switch (stype0) {
        case SType::BOOL:
        case SType::INT8:
        case SType::INT16:
        case SType::INT32: return make<int32_t>(std::move(cumsum), SType::INT32);
        case SType::INT64: return make<int64_t>(std::move(cumsum), SType::INT64);
        case SType::FLOAT32: return make<float>(std::move(cumsum), SType::FLOAT32);
        case SType::FLOAT64: return make<double>(std::move(cumsum), SType::FLOAT64);
        default:
            throw TypeError() << "Invalid column of type " << stype0 << " in " << repr();
      }
    }

    template <typename T>
    Column make(Column&& cumsum, SType stype0) const {
      cumsum.cast_inplace(stype0);
      return Column(new Latent_ColumnImpl(new Column_cumsum<T>(std::move(cumsum))));
    }
};


static py::oobj pyfn_cumsum(const py::XArgs& args) {
  auto cumsum = args[0].to_oobj();
  return PyFExpr::make(new FExpr_cumsum(as_fexpr(cumsum)));
}

DECLARE_PYFN(&pyfn_cumsum)
    ->name("cumsum")
    ->docs(doc_dt_cumsum)
    ->arg_names({"cumsum"})
    ->n_positional_args(1)
    ->n_required_args(1);

}}  // dt::expr
