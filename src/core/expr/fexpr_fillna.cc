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
#include "column/fillna.h"
#include "column/latent.h"
#include "documentation.h"
#include "expr/fexpr_func.h"
#include "expr/eval_context.h"
#include "expr/workframe.h"
#include "python/xargs.h"
#include "stype.h"


namespace dt {
  namespace expr {

    class FExpr_FillNA : public FExpr_Func {
      private:
        ptrExpr arg_;
        std::string direction_;

      public:
        FExpr_FillNA(ptrExpr &&arg, py::oobj direction)
            : arg_(std::move(arg))
             {if (direction.is_string()) {
              direction_ = direction.to_string();
             }
             else {
                throw TypeError() << "Parameter `direction` in fillna() should be "
                    "a string, instead got " << direction.typeobj();
             }
             bool bool_direction = (direction_=="down") || (direction_=="up");
             if (!bool_direction) {
              throw ValueError() << "The value for the parameter `direction` in fillna() "
                  "should be either `up` or `down`.";
             }
             }

        std::string repr() const override {
          std::string out = "fillna";
          out += '(';
          out += arg_->repr();
          out += ", direction=";
          out += direction_;
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

          bool bool_direction = direction_=="down";
          for (size_t i = 0; i < wf.ncols(); ++i) {
            Column coli = evaluate1(wf.retrieve_column(i), bool_direction, gby);
            wf.replace_column(i, std::move(coli));
          }
          return wf;
        }


      Column evaluate1(Column &&col, bool bool_direction, const Groupby &gby) const {
        SType stype = col.stype();
        switch (stype) {
          case SType::VOID:    return Column(new ConstNa_ColumnImpl(col.nrows()));
          case SType::BOOL:
          case SType::INT8:    return make<int8_t>(std::move(col), bool_direction, gby);
          case SType::INT16:   return make<int16_t>(std::move(col), bool_direction, gby);
          case SType::INT32:   return make<int32_t>(std::move(col), bool_direction, gby);
          case SType::INT64:   return make<int64_t>(std::move(col), bool_direction, gby);
          case SType::FLOAT32: return make<float>(std::move(col), bool_direction, gby);
          case SType::FLOAT64: return make<double>(std::move(col), bool_direction, gby);
          //case SType::STR32:   
          //case SType::STR64: return make<CString>(std::move(col), bool_direction, gby);  
          //case SType::DATE32:
          //case SType::DATE64:
          //case SType::TIME64:
          default: throw TypeError()
            << "Invalid column of type `" << stype << "` in " << repr();
        }
      }


        template <typename T>
        Column make(Column &&col, bool bool_direction, const Groupby &gby) const {
           return Column(new Latent_ColumnImpl(
             new FillNA_ColumnImpl<T>(std::move(col), bool_direction, gby)
           ));
        }
    };



    static py::oobj pyfn_fillna(const py::XArgs &args) {
      auto fillna = args[0].to_oobj();
      auto direction = args[1].to_oobj();
      return PyFExpr::make(new FExpr_FillNA(as_fexpr(fillna), direction));
    }


    DECLARE_PYFN(&pyfn_fillna)
        ->name("fillna")
        //->docs(doc_dt_fillna)
        ->arg_names({"column", "direction"})
        ->n_required_args(2)
        ->n_positional_args(1)
        ->n_positional_or_keyword_args(1);

  }  // namespace dt::expr
}    // namespace dt
