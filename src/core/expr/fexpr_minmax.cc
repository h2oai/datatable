//------------------------------------------------------------------------------
// Copyright 2023 H2O.ai
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
#include "column/minmax.h"
#include "documentation.h"
#include "expr/fexpr_reduce_unary.h"
#include "expr/eval_context.h"
#include "expr/workframe.h"
#include "python/xargs.h"
#include "stype.h"
namespace dt {
namespace expr {


template <bool MIN>
class FExpr_MinMax : public FExpr_ReduceUnary {
  public:
    using FExpr_ReduceUnary::FExpr_ReduceUnary;


    std::string name() const override {
      return MIN? "min"
                : "max";
    }


    Column evaluate1(Column&& col, const Groupby& gby, bool is_grouped) const override {
      SType stype = col.stype();

      switch (stype) {
        case SType::VOID:
          return Column(new ConstNa_ColumnImpl(gby.size(), stype));
        case SType::BOOL:
        case SType::INT8:
          return make<int8_t>(std::move(col), SType::INT8, gby, is_grouped);
        case SType::INT16:
          return make<int16_t>(std::move(col), SType::INT16, gby, is_grouped);
        case SType::INT32:
        return make<int32_t>(std::move(col), SType::INT32, gby, is_grouped);
        case SType::DATE32:
          return make<int32_t>(std::move(col), SType::DATE32, gby, is_grouped);
        case SType::INT64:
        return make<int64_t>(std::move(col), SType::INT64, gby, is_grouped);
        case SType::TIME64:
          return make<int64_t>(std::move(col), SType::TIME64, gby, is_grouped);
        case SType::FLOAT32:
          return make<float>(std::move(col), SType::FLOAT32, gby, is_grouped);
        case SType::FLOAT64:
          return make<double>(std::move(col), SType::FLOAT64, gby, is_grouped);
        default:
          throw TypeError()
            << "Invalid column of type `" << stype << "` in " << repr();
      }
    }


    template <typename T>
    Column make(Column&& col, SType stype, const Groupby& gby, bool is_grouped) const {
      return is_grouped? std::move(col)
                       : Column(new Latent_ColumnImpl(new MinMax_ColumnImpl<T, MIN>(
                           std::move(col), stype, gby
                         )));

    }
};


static py::oobj pyfn_min(const py::XArgs &args) {
  auto min = args[0].to_oobj();
  return PyFExpr::make(new FExpr_MinMax<true>(as_fexpr(min)));
}


static py::oobj pyfn_max(const py::XArgs &args) {
  auto max = args[0].to_oobj();
  return PyFExpr::make(new FExpr_MinMax<false>(as_fexpr(max)));
}


DECLARE_PYFN(&pyfn_min)
    ->name("min")
    ->docs(doc_dt_min)
    ->arg_names({"cols"})
    ->n_positional_args(1)
    ->n_required_args(1);

DECLARE_PYFN(&pyfn_max)
    ->name("max")
    ->docs(doc_dt_max)
    ->arg_names({"cols"})
    ->n_positional_args(1)
    ->n_required_args(1);


}}  // dt::expr
