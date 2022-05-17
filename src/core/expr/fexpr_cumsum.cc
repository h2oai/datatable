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
#include "column/cumsum.h"
#include "column/latent.h"
#include "documentation.h"
#include "expr/fexpr_func.h"
#include "expr/eval_context.h"
#include "expr/workframe.h"
#include "python/xargs.h"
#include "stype.h"


namespace dt {
namespace expr {

class FExpr_cumsum : public FExpr_Func {
  private:
    ptrExpr arg_;

  public:
    FExpr_cumsum(ptrExpr&& arg)
      : arg_(std::move(arg)) {}

    std::string repr() const override{
      std::string out = "cumsum(";
      out += arg_->repr();
      out += ')';
      return out;
    }


    Workframe evaluate_n(EvalContext& ctx) const override{
      Workframe wf = arg_->evaluate_n(ctx);
      Groupby gby = Groupby::single_group(wf.nrows());

      if (ctx.has_groupby()) {
        wf.increase_grouping_mode(Grouping::GtoALL);
        gby = ctx.get_groupby();
      }

      for (size_t i = 0; i < wf.ncols(); ++i) {
        Column coli = evaluate1(wf.retrieve_column(i), gby);
        wf.replace_column(i, std::move(coli));
      }
      return wf;
    }


    Column evaluate1(Column&& col, const Groupby& gby) const {
      SType stype = col.stype();
      switch (stype) {
        case SType::VOID:
        case SType::BOOL:
        case SType::INT8:
        case SType::INT16:
        case SType::INT32:
        case SType::INT64: return make<int64_t>(std::move(col), SType::INT64, gby);
        case SType::FLOAT32: return make<float>(std::move(col), SType::FLOAT32, gby);
        case SType::FLOAT64: return make<double>(std::move(col), SType::FLOAT64, gby);
        default: throw TypeError()
          << "Invalid column of type " << stype << " in " << repr();
      }
    }


    template <typename T>
    Column make(Column&& col, SType stype, const Groupby& gby) const {
      if (col.stype() == SType::VOID) {
        return Column(new ConstInt_ColumnImpl(col.nrows(), 0, stype));
      } else {
        col.cast_inplace(stype);
        return Column(new Latent_ColumnImpl(
          new Cumsum_ColumnImpl<T>(std::move(col), gby)
        ));
      }
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
