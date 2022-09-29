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
#include "column/nth.h"
#include "documentation.h"
#include "expr/fexpr_func.h"
#include "expr/eval_context.h"
#include "python/xargs.h"
#include "expr/fnary/fnary.h"
#include "column/isna.h"
namespace dt {
namespace expr {


class FExpr_Nth : public FExpr_Func {
  private:
    ptrExpr arg_;
    int32_t n_;
    std::string dropna_;
    size_t : 32;

  public:
    FExpr_Nth(ptrExpr&& arg, int32_t n, std::string dropna)
      : arg_(std::move(arg)),
        n_(n)
        {}

      out += '(';
      out += arg_->repr();
      out += ", n=";
      out += std::to_string(n_);
    Workframe evaluate_n(EvalContext &ctx) const override {
      Workframe inputs = arg_->evaluate_n(ctx);
      Workframe outputs(ctx);
      Groupby gby = ctx.get_groupby();

      if (!gby) gby = Groupby::single_group(ctx.nrows()); 

      for (size_t i = 0; i < inputs.ncols(); ++i) {
        auto coli = inputs.retrieve_column(i);
        outputs.add_column(
          evaluate1(std::move(coli), gby, n_),
          inputs.retrieve_name(i),
        );
      }
      return outputs;
    }


    Column evaluate1(Column&& col, const Groupby& gby, const int32_t n) const {
      SType stype = col.stype();
      switch (stype) {
        case SType::VOID:    return Column(new ConstNa_ColumnImpl(gby.size()));
        case SType::DATE32:
        case SType::INT32:   return make<int32_t>(std::move(col), gby, n);
        case SType::TIME64:
        case SType::INT64:   return make<int64_t>(std::move(col), gby, n);
        case SType::FLOAT32: return make<float>(std::move(col), gby, n);
        case SType::FLOAT64: return make<double>(std::move(col), gby, n);
        case SType::STR32:   return make<CString>(std::move(col), gby, n);
        case SType::STR64:   return make<CString>(std::move(col), gby, n);
        default:
          throw TypeError()
            << "Invalid column of type `" << stype << "` in " << repr();
      }
    }


    template <typename T>
    Column make(Column&& col, const Groupby& gby, int32_t n) const {
      return Column(new Nth_ColumnImpl<T>(std::move(col), gby, n));
<<<<<<< HEAD
=======
    }

};

>>>>>>> 5a379979 (skeleton for dropna logic)

static py::oobj pyfn_nth(const py::XArgs& args) {
  auto cols = args[0].to_oobj();
  auto n = args[1].to<int32_t>(0);
<<<<<<< HEAD

  return PyFExpr::make(new FExpr_Nth(as_fexpr(cols), n));

=======
  auto dropna = args[2].to<std::string>("None");
  bool dropna_check = dropna == "any" || dropna == "all" || dropna == "None";
  if (!dropna_check) {
    throw ValueError() << "Parameter `dropna` in nth() should be "
        "either `None`, `any`, or `all`";
  }
  return PyFExpr::make(new FExpr_Nth(as_fexpr(cols), n, dropna));
>>>>>>> 5a379979 (skeleton for dropna logic)

}


<<<<<<< HEAD
=======
DECLARE_PYFN(&pyfn_nth)
    ->name("nth")
    //->docs(doc_dt_nth)
    ->arg_names({"cols", "n", "dropna"})
    ->n_positional_args(1)
    ->n_positional_or_keyword_args(2)
    ->n_required_args(1);


>>>>>>> 5a379979 (skeleton for dropna logic)
}}  // dt::expr
