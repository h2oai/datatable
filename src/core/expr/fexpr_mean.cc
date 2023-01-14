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
#include "column/latent.h"
#include "column/mean.h"
#include "documentation.h"
#include "expr/fexpr_func.h"
#include "expr/eval_context.h"
#include "expr/workframe.h"
#include "python/xargs.h"
#include "stype.h"
#include <iostream>
namespace dt {
namespace expr {


class FExpr_Mean : public FExpr_Func {
  private:
    ptrExpr arg_;

  public:
    FExpr_Mean(ptrExpr &&arg)
      : arg_(std::move(arg)) {}

    std::string repr() const override {
      std::string out = "mean";
      out += '(';
      out += arg_->repr();
      out += ')';
      return out;
    }


    Workframe evaluate_n(EvalContext &ctx) const override {
      Workframe outputs(ctx);
      Workframe wf = arg_->evaluate_n(ctx);
      Groupby gby = ctx.get_groupby();
      bool has_by = ctx.has_groupby();


      if (!gby) {
        gby = Groupby::single_group(wf.nrows());
      } 

      for (size_t i = 0; i < wf.ncols(); ++i) {
        bool is_grouped = ctx.has_group_column(
                            wf.get_frame_id(i),
                            wf.get_column_id(i)
                          );
        Column coli = evaluate1(wf.retrieve_column(i), gby, has_by, is_grouped);        
        outputs.add_column(std::move(coli), wf.retrieve_name(i), Grouping::GtoONE);         
      }
        
      return outputs;
    }


    Column evaluate1(Column &&col, const Groupby& gby, bool has_by, bool is_grouped) const {
      SType stype = col.stype();

      switch (stype) {
        case SType::VOID: {
          std::cout<<has_by<<std::endl;
          Column coli = has_by? Column(new ConstNa_ColumnImpl(gby.size(), SType::FLOAT64))
                              : Column(new ConstNa_ColumnImpl(gby.size()));
          return coli;
        }
          
        case SType::BOOL:
        case SType::INT8:
        case SType::INT16:
        case SType::INT32:        
        case SType::INT64:
        case SType::FLOAT64:
          return make<double>(std::move(col), SType::FLOAT64, gby, is_grouped);
        case SType::TIME64: {
          Column coli = make<double>(std::move(col), SType::FLOAT64, gby, is_grouped);
          coli.cast_inplace(SType::TIME64);
          return coli;
        }           
        case SType::FLOAT32:
          return make<float>(std::move(col), SType::FLOAT32, gby, is_grouped);
        
        default:
          throw TypeError()
            << "Invalid column of type `" << stype << "` in " << repr();
      }
    }


    template <typename T>
    Column make(Column &&col, SType stype, const Groupby& gby, bool is_grouped) const {
      col.cast_inplace(stype);
      return Column(new Latent_ColumnImpl(new Mean_ColumnImpl<T>(
        std::move(col), gby, is_grouped
      )));
    }
};


static py::oobj pyfn_mean(const py::XArgs &args) {
  auto mean = args[0].to_oobj();
  return PyFExpr::make(new FExpr_Mean(as_fexpr(mean)));
}

DECLARE_PYFN(&pyfn_mean)
    ->name("mean")
    ->docs(doc_dt_min)
    ->arg_names({"cols"})
    ->n_positional_args(1)
    ->n_required_args(1);
}}  // dt::expr
