#include "expr/fexpr_func.h"
#include "column/virtual.h"
#include "expr/eval_context.h"
#include "expr/workframe.h"
#include "stype.h"
#include "python/xargs.h"
#include "documentation.h"
#include <numeric>
#include <vector>

namespace dt {
namespace expr {

template <typename T>
class Column_cumsum : public Virtual_ColumnImpl{
    private:
      Column acol_;

    public:
      Column_cumsum(Column&& a)
        : Virtual_ColumnImpl(a.nrows(), a.stype()),
          acol_(std::move(a))

    {
      xassert(acol_.can_be_read_as<T>());
    }

      ColumnImpl* clone() const override {
          return new Column_cumsum(Column(acol_));
      }

      size_t n_children() const noexcept override {
      return 1;
    }

    const Column& child(size_t) const override {
      return acol_;
    }


    bool get_element(size_t i, T* out) const override {
      bool out_valid = false;
      T tmp = 0;
      for (size_t i = 0; i < acol_.nrows(); ++i){
        T a;
        bool isvalid = acol_.get_element(i, &a);
        if (isvalid){
          out_valid = true;
          tmp = a + tmp;
          *out = tmp;
        }
      }
      
      return out_valid;
     }

};



class FExpr_cumsum : public FExpr_Func {
  private:
    ptrExpr a_;

  public:
    FExpr_cumsum(ptrExpr&& a)
      : a_(std::move(a)) {}

    std::string repr() const override{
        std::string out = "cumsum(";
        out += a_->repr();
        out += ')';
        return out;
    };
    Workframe evaluate_n(EvalContext& ctx) const override {
        Workframe awf = a_->evaluate_n(ctx);

        auto gmode = awf.get_grouping_mode();
        Workframe outputs(ctx);
        for (size_t i = 0; i < awf.ncols(); ++i) {
            Column rescol = evaluate1(awf.retrieve_column(i));
            outputs.add_column(std::move(rescol), std::string(), gmode);
         }

        return outputs;
    };

    Column evaluate1(Column&& a) const {

  switch (a.stype()) {
    case SType::BOOL:
    case SType::INT8:
    case SType::INT16:
    case SType::INT32: return make<int32_t>(std::move(a));
    case SType::INT64: return make<int64_t>(std::move(a));
    case SType::FLOAT32: return make<float>(std::move(a));
    case SType::FLOAT64: return make<double>(std::move(a));
    default:
        throw TypeError() 
        << "Function datatable.cumsum() cannot be applied to a column "
               "of type `" << a.stype() << "`";
  }
}

template <typename T>
Column make(Column&& a) const {
  return Column(new Column_cumsum<T>(std::move(a)));
}
};


static py::oobj py_cumsum(const py::XArgs& args) {
  auto a = args[0].to_oobj();
  return PyFExpr::make(new FExpr_cumsum(as_fexpr(a)));
}

DECLARE_PYFN(&py_cumsum)
    ->name("cumsum")
    // ->docs(dt::doc_dt_cumsum)
    ->arg_names({"a"})
    ->n_positional_args(1)
    ->n_required_args(1);


}}