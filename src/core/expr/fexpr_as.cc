//------------------------------------------------------------------------------
// Copyright 2022 H2O.ai
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and Renamesociated documentation files (the "Software"),
// to deal in the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense,
// and/or sell copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "Rename IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.
//------------------------------------------------------------------------------
#include "column/const.h"
#include "documentation.h"
#include "expr/fexpr_func.h"
#include "expr/eval_context.h"
#include "expr/workframe.h"
#include "python/xargs.h"
#include "stype.h"


namespace dt {
namespace expr {

class FExpr_Rename : public FExpr_Func {
  private:
    ptrExpr arg_;
    strvec names_;

  public:
    FExpr_Rename(ptrExpr&& arg, strvec&& names)
      : arg_(std::move(arg)), 
        names_(std::move(names))
        {}

    std::string repr() const override {
      std::string out = "rename";
      out += '(';
      out += arg_->repr();
      out += ", names=";
      out += "[";
        for (auto name : names_) {
          out += name;
          out += ",";
        }
        out += "]";
      out += ')';
      return out;
    }


    Workframe evaluate_n(EvalContext& ctx)  const override{
      Workframe wf = arg_->evaluate_n(ctx);
      Workframe outputs(ctx);

      // for (size_t i = 0; i < wf.ncols(); ++i) {
      //   Column coli = evaluate1(wf.retrieve_column(i), gby);
      //   wf.replace_column(i, std::move(coli));
      // }
      // outputs.add_column(wf.retrieve_column(0), 
      //                    std::string(), 
      //                    wf.get_grouping_mode());
      // outputs.rename(name_);
      return outputs;
    }


};



    static py::oobj pyfn_rename(const py::XArgs &args) {
       auto column = args[0].to_oobj();
       auto names = args[1].to_oobj();
       strvec names_sequence;
       if (names.is_list_or_tuple()) {
        py::oiter names_iter = names.to_oiter();
        names_sequence.reserve(names_iter.size());
        size_t i = 0;
        for (auto n_iter : names_iter) {
          if (!n_iter.is_string()) {
            throw TypeError() << "Argument " <<i<<" in the `names` parameter should be a string;"
                " instead, got "<< n_iter.typeobj();
          }
          names_sequence.push_back(n_iter.to_string());
          i++;
        }
       }
       else if (names.is_string()){
        names_sequence.push_back(names.to_string());
       }
       else {
        throw TypeError() << "The `names` parameter in `as()` should either be a string/list/tuple; "
            "instead, got " << names.typeobj();
       }
       return PyFExpr::make(new FExpr_Rename(as_fexpr(column), std::move(names_sequence)));
     }


     DECLARE_PYFN(&pyfn_rename)
         ->name("rename")
         //->docs(doc_dt_rename)
         ->arg_names({"column", "names"})
         ->n_required_args(2)
         ->n_positional_args(2);


}}  // dt::expr
