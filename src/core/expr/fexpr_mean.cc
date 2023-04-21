//------------------------------------------------------------------------------
// Copyright 2022-2023 H2O.ai
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
#include "column/latent.h"
#include "column/mean.h"
#include "documentation.h"
#include "expr/fexpr_reduce_unary.h"
#include "expr/eval_context.h"
#include "expr/workframe.h"
#include "python/xargs.h"
#include "stype.h"
namespace dt {
namespace expr {


class FExpr_Mean : public FExpr_ReduceUnary {
  public:
    using FExpr_ReduceUnary::FExpr_ReduceUnary;


    std::string name() const override {
      return "mean";
    }


    Column evaluate1(Column&& col, const Groupby& gby, bool is_grouped) const override{
      SType stype = col.stype();
      Column col_out;

      switch (stype) {
        case SType::VOID: return Column(new ConstNa_ColumnImpl(
                            gby.size(), SType::FLOAT64
                          ));
        case SType::BOOL:
        case SType::INT8:
        case SType::INT16:
        case SType::INT32:        
        case SType::INT64:
        case SType::DATE32:
        case SType::TIME64:
        case SType::FLOAT64:
          col_out = make<double>(std::move(col), SType::FLOAT64, gby, is_grouped);
          break;
        case SType::FLOAT32:
          col_out = make<float>(std::move(col), SType::FLOAT32, gby, is_grouped);
          break;
        default:
          throw TypeError()
            << "Invalid column of type `" << stype << "` in " << repr();
      }

      if (stype == SType::DATE32 || stype == SType::TIME64) {
        col_out.cast_inplace(stype);
      }

      return col_out;
    }


    template <typename T>
    Column make(Column&& col, SType stype, const Groupby& gby, bool is_grouped) const {
      col.cast_inplace(stype);

      return is_grouped? std::move(col)
                       : Column(new Latent_ColumnImpl(new Mean_ColumnImpl<T>(
                           std::move(col), stype, gby
                         )));
    }
};


static py::oobj pyfn_mean(const py::XArgs &args) {
  auto mean = args[0].to_oobj();
  return PyFExpr::make(new FExpr_Mean(as_fexpr(mean)));
}

DECLARE_PYFN(&pyfn_mean)
    ->name("mean")
    ->docs(doc_dt_mean)
    ->arg_names({"cols"})
    ->n_positional_args(1)
    ->n_required_args(1);


}}  // dt::expr
