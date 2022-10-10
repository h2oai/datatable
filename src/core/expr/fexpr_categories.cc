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
#include "_dt.h"
#include "documentation.h"
#include "column/const.h"
#include "expr/eval_context.h"
#include "expr/fexpr_func.h"
#include "python/xargs.h"
namespace dt {
namespace expr {


//------------------------------------------------------------------------------
// FExpr_categories
//------------------------------------------------------------------------------

class FExpr_Categories : public FExpr_Func {
  private:
    ptrExpr arg_;

  public:
    FExpr_Categories(ptrExpr &&arg)
        : arg_(std::move(arg)) {}


    std::string repr() const override {
      std::string out = "categories";
      out += '(';
      out += arg_->repr();
      out += ')';
      return out;
    }


    Workframe evaluate_n(EvalContext &ctx) const override {
      Workframe wf = arg_->evaluate_n(ctx);

      for (size_t i = 0; i < wf.ncols(); ++i) {
        Column col = wf.retrieve_column(i);
        if (!col.type().is_categorical()) {
          throw TypeError()
            << "Invalid column of type `" << col.stype()
            << "` in " << repr();
        }
        Column categories;
        if (col.n_children()) {
          categories = Column(col.child(0));
        } else {
          categories = Const_ColumnImpl::make_na_column(1);
        }
        wf.replace_column(i, std::move(categories));
      }
      return wf;
    }

};



//------------------------------------------------------------------------------
// Python-facing `categories()` function
//------------------------------------------------------------------------------

static py::oobj pyfn_categories(const py::XArgs& args) {
  auto cols = args[0].to_oobj();
  return PyFExpr::make(new FExpr_Categories(as_fexpr(cols)));
}


DECLARE_PYFN(&pyfn_categories)
    ->name("categories")
    ->docs(doc_dt_categories)
    ->arg_names({"cols"})
    ->n_positional_args(1)
    ->n_required_args(1);


}}  // dt::expr
