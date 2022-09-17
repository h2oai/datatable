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
#include "expr/eval_context.h"
#include "expr/fexpr_func.h"
#include "python/xargs.h"
namespace dt {
namespace expr {


class FExpr_Alias : public FExpr_Func {
  private:
    ptrExpr arg_;
    strvec names_;

  public:
    FExpr_Alias(ptrExpr&& arg, strvec&& names) :
      arg_(std::move(arg)),
      names_(std::move(names))
      {}

    std::string repr() const override {
      std::string out = "alias";
      out += '(';
      out += arg_->repr();
      out += ", [";
      for (auto name : names_) {
        out += name;
        out += ",";
      }
      out += "]";
      out += ')';
      return out;
    }

    Workframe evaluate_n(EvalContext& ctx) const override {
      Workframe wf = arg_->evaluate_n(ctx);
      if (wf.ncols() != names_.size()) {
        throw ValueError()
          << "The number of columns does not match the number of names: "
          << wf.ncols() << " vs " << names_.size();
      }

      Workframe wf_out(ctx);
      auto gmode = wf.get_grouping_mode();

      for (size_t i = 0; i < wf.ncols(); ++i) {
        Workframe arg_out(ctx);
        Column col = wf.retrieve_column(i);
        arg_out.add_column(std::move(col), std::string(names_[i]), gmode);
        wf_out.cbind( std::move(arg_out) );
      }

      return wf_out;
    }
};



//------------------------------------------------------------------------------
// Python-facing `alias()` function
//------------------------------------------------------------------------------

static py::oobj pyfn_alias(const py::XArgs& args) {
  auto cols = args[0].to_oobj();
  auto names = args[1].to_oobj();
  strvec names_vec;

  if (names.is_string()) {
    names_vec.push_back(names.to_string());
  } else if (names.is_list_or_tuple()) {
    py::oiter names_iter = names.to_oiter();
    names_vec.reserve(names_iter.size());

    for (auto name : names_iter) {
      if (name.is_string()) {
        names_vec.emplace_back(name.to_string());
      } else {
        throw TypeError()
          << "All elements of the list/tuple of the `names` in `datatable.alias()` "
          << "must be strings, instead got: "<< name.typeobj();
      }
    }
  } else {
    throw TypeError()
      << "Parameter `names` in `datatable.alias()` must be a string, or "
      << "a list/tuple of strings, instead got: "<< names.typeobj();
  }

  return PyFExpr::make(new FExpr_Alias(as_fexpr(cols), std::move(names_vec)));
}


DECLARE_PYFN(&pyfn_alias)
    ->name("alias")
    // ->docs(doc_dt_alias_type)
    ->arg_names({"cols", "names"})
    ->n_positional_args(2)
    ->n_required_args(2);


}}  // dt::expr
