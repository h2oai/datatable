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
#include "column/sumprod.h"
#include "documentation.h"
#include "expr/fexpr_reduce_unary.h"
#include "expr/eval_context.h"
#include "expr/workframe.h"
#include "python/xargs.h"
#include "stype.h"
namespace dt {
namespace expr {


template <bool SUM>
class FExpr_SumProd : public FExpr_ReduceUnary {
  public:
    using FExpr_ReduceUnary::FExpr_ReduceUnary;


    std::string name() const override {
      return SUM? "sum"
                : "prod";
    }


    Column evaluate1(Column&& col, const Groupby& gby, bool is_grouped) const override {
      SType stype = col.stype();

      switch (stype) {
        case SType::VOID:
          return Column(new ConstInt_ColumnImpl(gby.size(), !SUM, SType::INT64));
        case SType::BOOL:
        case SType::INT8:
        case SType::INT16:
        case SType::INT32:
        case SType::INT64:
          return make<int64_t>(std::move(col), gby, is_grouped);
        case SType::FLOAT32:
          return make<float>(std::move(col), gby, is_grouped);
        case SType::FLOAT64:
          return make<double>(std::move(col), gby, is_grouped);
        default:
          throw TypeError()
            << "Invalid column of type `" << stype << "` in " << repr();
      }
    }


    template <typename T>
    Column make(Column &&col, const Groupby& gby, bool is_grouped) const {
      if (is_grouped) {
        return Column(new Latent_ColumnImpl(new SumProd_ColumnImpl<T, T, SUM, true>(
          std::move(col), gby
        )));
      } else {
        return Column(new Latent_ColumnImpl(new SumProd_ColumnImpl<T, T, SUM, false>(
          std::move(col), gby
        )));
      }
    }
};


static py::oobj pyfn_sum(const py::XArgs &args) {
  auto sum = args[0].to_oobj();
  return PyFExpr::make(new FExpr_SumProd<true>(as_fexpr(sum)));
}


static py::oobj pyfn_prod(const py::XArgs &args) {
  auto prod = args[0].to_oobj();
  return PyFExpr::make(new FExpr_SumProd<false>(as_fexpr(prod)));
}


DECLARE_PYFN(&pyfn_sum)
    ->name("sum")
    ->docs(doc_dt_sum)
    ->arg_names({"cols"})
    ->n_positional_args(1)
    ->n_required_args(1);

DECLARE_PYFN(&pyfn_prod)
    ->name("prod")
    ->docs(doc_dt_prod)
    ->arg_names({"cols"})
    ->n_positional_args(1)
    ->n_required_args(1);


}}  // dt::expr
