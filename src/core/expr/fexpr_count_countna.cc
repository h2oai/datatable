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
#include "column/countna.h"
#include "column/count_all_rows.h"
#include "documentation.h"
#include "expr/fexpr_func.h"
#include "expr/eval_context.h"
#include "expr/workframe.h"
#include "python/xargs.h"
#include "stype.h"
namespace dt {
namespace expr {

template<bool COUNTNA>
class FExpr_CountNA : public FExpr_Func {
  private:
    ptrExpr arg_;

  public:
    FExpr_CountNA(ptrExpr &&arg)
      : arg_(std::move(arg)) {}

    std::string repr() const override {
      std::string out = COUNTNA? "countna" : "count";
      out += '(';
      if (arg_->get_expr_kind() != Kind::None) out += arg_->repr();      
      out += ')';
      return out;
    }


    Workframe evaluate_n(EvalContext &ctx) const override {
      Workframe outputs(ctx);
      Workframe wf = arg_->evaluate_n(ctx);
      Groupby gby = ctx.get_groupby();
      // this covers scenarios where
      // we dont care about the presence or absence of NAs
      // we just want the total number of rows
      bool count_all_rows = arg_->get_expr_kind() == Kind::None;

      if (count_all_rows && !COUNTNA) {
        Column coli;
        auto value = static_cast<int64_t>(ctx.nrows());
        if (gby){
          coli = Const_ColumnImpl::make_int_column(value, 1, SType::INT64);
          coli = Column(new Latent_ColumnImpl(new CountAllRows_ColumnImpl(std::move(coli), gby)));
        } else{            
            coli = Const_ColumnImpl::make_int_column(1, value, SType::INT64);
          }
        outputs.add_column(std::move(coli), "count", Grouping::GtoONE);
        return outputs;
      }

      if (!gby) {
        gby = Groupby::single_group(wf.nrows());
      } 

      if (count_all_rows && COUNTNA){
        Column coli = Const_ColumnImpl::make_int_column(gby.size(), 0, SType::INT64);
        outputs.add_column(std::move(coli), std::string(), Grouping::GtoONE);
        return outputs;
      }

      for (size_t i = 0; i < wf.ncols(); ++i) {
        bool is_grouped = ctx.has_group_column(
                            wf.get_frame_id(i),
                            wf.get_column_id(i)
                          );
        Column coli = wf.retrieve_column(i);   
        if (COUNTNA && !ctx.has_groupby() && (coli.stype() == SType::VOID)) {
          int64_t nrows = static_cast<int64_t>(ctx.nrows());
          coli = Const_ColumnImpl::make_int_column(1, nrows, SType::INT64);
        } else {
            coli = evaluate1(std::move(coli), gby, is_grouped);
          }
        outputs.add_column(std::move(coli), wf.retrieve_name(i), Grouping::GtoONE);                         
      }        
      return outputs;
    }


    Column evaluate1(Column &&col, const Groupby& gby, bool is_grouped) const {
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

    template <typename T_IN>
    Column make(Column &&col, const Groupby& gby, bool is_grouped) const {
      if (is_grouped) {
        return Column(new Latent_ColumnImpl(new Count_ColumnImpl<T_IN, COUNTNA, true>(
          std::move(col), gby
        )));
      } else {
        return Column(new Latent_ColumnImpl(new Count_ColumnImpl<T_IN, COUNTNA, false>(
          std::move(col), gby
        )));
      }
    }
};

static py::oobj pyfn_count(const py::XArgs &args) {
  auto count = args[0].to_oobj_or_none();
  return PyFExpr::make(new FExpr_CountNA<false>(as_fexpr(count)));
}

static py::oobj pyfn_countna(const py::XArgs &args) {
  auto countna = args[0].to_oobj();
  return PyFExpr::make(new FExpr_CountNA<true>(as_fexpr(countna)));
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
    ->n_positional_args(1)
    ->n_required_args(1);


}}  // dt::expr
