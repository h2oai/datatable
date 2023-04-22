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
#include "column/count.h"
#include "documentation.h"
#include "expr/fexpr_reduce_unary.h"
#include "expr/eval_context.h"
#include "expr/workframe.h"
#include "python/xargs.h"
#include "stype.h"
namespace dt {
namespace expr {


template<bool COUNTNA>
class FExpr_CountUnary : public FExpr_ReduceUnary {
  public:
    using FExpr_ReduceUnary::FExpr_ReduceUnary;


    std::string name() const override {
      return COUNTNA? "countna"
                    : "count";
    }


    Column evaluate1(Column&& col, const Groupby& gby, bool is_grouped) const override {
      SType stype = col.stype();
      switch (stype) {
        case SType::VOID:
        case SType::BOOL:
        case SType::INT8:
          return make<int8_t>(std::move(col), gby, is_grouped);
        case SType::INT16:
          return make<int16_t>(std::move(col), gby, is_grouped);
        case SType::DATE32: 
        case SType::INT32:
          return make<int32_t>(std::move(col), gby, is_grouped);
        case SType::TIME64: 
        case SType::INT64:
          return make<int64_t>(std::move(col), gby, is_grouped);
        case SType::FLOAT32:
          return make<float>(std::move(col), gby, is_grouped);
        case SType::FLOAT64:
          return make<double>(std::move(col), gby, is_grouped);
        case SType::STR32:
        case SType::STR64:
          return make<CString>(std::move(col), gby, is_grouped);
        default:
          throw TypeError()
            << "Invalid column of type `" << stype << "` in " << repr();
      }
    }


    template <typename T>
    Column make(Column&& col, const Groupby& gby, bool is_grouped) const {
      if (is_grouped) {
        return Column(new Latent_ColumnImpl(new CountUnary_ColumnImpl<T, COUNTNA, true>(
          std::move(col), gby, SType::INT64
        )));
      } else {
        return Column(new Latent_ColumnImpl(new CountUnary_ColumnImpl<T, COUNTNA, false>(
          std::move(col), gby, SType::INT64
        )));
      }
    }

};



template<bool COUNTNA>
class FExpr_CountNullary : public FExpr_Func {
  public:
    std::string repr() const override {
      std::string out = COUNTNA? "countna()"
                               : "count()";
      return out;
    }


    Workframe evaluate_n(EvalContext &ctx) const override {
      Workframe wf(ctx);
      Groupby gby = ctx.get_groupby();
      Column col;

      if (COUNTNA) {
        col = Const_ColumnImpl::make_int_column(gby.size(), 0, SType::INT64);
        wf.add_column(std::move(col), "countna", Grouping::GtoONE);
        return wf;
      }

      if (ctx.has_groupby()) {
        col = Column(new Latent_ColumnImpl(new CountNullary_ColumnImpl(gby)));
      } else {
        auto value = static_cast<int64_t>(ctx.nrows());
        col = Const_ColumnImpl::make_int_column(1, value, SType::INT64);
      }
      wf.add_column(std::move(col), "count", Grouping::GtoONE);
      return wf;
    }

};


static py::oobj pyfn_count(const py::XArgs &args) {
  auto arg = args[0].to_oobj_or_none();
  return arg.is_none()? PyFExpr::make(new FExpr_CountNullary<false>())
                      : PyFExpr::make(new FExpr_CountUnary<false>(as_fexpr(arg)));
}

static py::oobj pyfn_countna(const py::XArgs &args) {
  auto arg = args[0].to_oobj_or_none();
  return arg.is_none()? PyFExpr::make(new FExpr_CountNullary<true>())
                      : PyFExpr::make(new FExpr_CountUnary<true>(as_fexpr(arg)));
}


DECLARE_PYFN(&pyfn_count)
    ->name("count")
    ->docs(doc_dt_count)
    ->arg_names({"cols"})
    ->n_positional_args(1);


DECLARE_PYFN(&pyfn_countna)
    ->name("countna")
    ->docs(doc_dt_countna)
    ->arg_names({"cols"})
    ->n_positional_args(1);


}}  // dt::expr
