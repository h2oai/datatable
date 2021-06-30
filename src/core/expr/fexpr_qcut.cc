//------------------------------------------------------------------------------
// Copyright 2020 H2O.ai
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
#include "column/latent.h"
#include "column/sentinel_fw.h"
#include "column/qcut.h"
#include "documentation.h"
#include "expr/eval_context.h"
#include "expr/fexpr_func.h"
#include "frame/py_frame.h"
#include "parallel/api.h"
#include "python/xargs.h"
namespace dt {
namespace expr {



//------------------------------------------------------------------------------
// FExpr_Qcut
//------------------------------------------------------------------------------

class FExpr_Qcut : public FExpr_Func {
  private:
    ptrExpr arg_;
    py::oobj py_nquantiles_;

  public:
    FExpr_Qcut(py::oobj arg, py::robj py_nquantiles)
      : arg_(as_fexpr(arg)),
        py_nquantiles_(py_nquantiles)
    {}

    std::string repr() const override {
      std::string out = "qcut(";
      out += arg_->repr();
      if (!py_nquantiles_.is_none()) {
        out += ", nquantiles=";
        out += py_nquantiles_.repr().to_string();
      }
      out += ")";
      return out;
    }


    Workframe evaluate_n(EvalContext& ctx) const override {
      if (ctx.has_groupby()) {
        throw NotImplError() << "qcut() cannot be used in a groupby context";
      }

      Workframe wf = arg_->evaluate_n(ctx);
      const size_t ncols = wf.ncols();

      int32vec nquantiles(ncols);
      bool defined_nquantiles = !py_nquantiles_.is_none();
      bool nquantiles_list_or_tuple = py_nquantiles_.is_list_or_tuple();

      if (nquantiles_list_or_tuple) {
        py::oiter py_nquantiles = py_nquantiles_.to_oiter();
        if (py_nquantiles.size() != ncols) {
          throw ValueError() << "When `nquantiles` is a list or a tuple, its "
            << "length must be the same as the number of input columns, i.e. `"
            << ncols << "`, instead got: `" << py_nquantiles.size() << "`";

        }

        size_t i = 0;
        for (auto py_nquantile : py_nquantiles) {
          int32_t nquantile = py_nquantile.to_int32_strict();
          if (nquantile <= 0) {
            throw ValueError() << "All elements in `nquantiles` must be positive, "
              << "got `nquantiles[" << i << "`]: `" << nquantile << "`";
          }

          nquantiles[i++] = nquantile;
        }
        xassert(i == ncols);
      }
      else {
        int32_t nquantiles_default = 10;
        if (defined_nquantiles) {
          nquantiles_default = py_nquantiles_.to_int32_strict();
          if (nquantiles_default <= 0) {
            throw ValueError() << "Number of quantiles must be positive, "
              "instead got: `" << nquantiles_default << "`";
          }
        }

        for (size_t i = 0; i < ncols; ++i) {
          nquantiles[i] = nquantiles_default;
        }
      }

      // Qcut workframe in-place
      for (size_t i = 0; i < ncols; ++i) {
        Column coli = wf.retrieve_column(i);

        if (coli.ltype() == dt::LType::STRING || coli.ltype() == dt::LType::OBJECT)
        {
          throw TypeError() << "`qcut()` cannot be applied to "
            << "string or object columns, instead column `" << i
            << "` has an stype: `" << coli.stype() << "`";
        }

        coli = Column(new Latent_ColumnImpl(new Qcut_ColumnImpl(
                 std::move(coli), nquantiles[i]
               )));
        wf.replace_column(i, std::move(coli));
      }

      return wf;
    }
};




//------------------------------------------------------------------------------
// Python-facing `qcut()` function
//------------------------------------------------------------------------------

static py::oobj pyfn_qcut(const py::XArgs& args) {
  auto cols       = args[0].to_oobj();
  auto nquantiles = args[1].to<py::oobj>(py::None());
  return PyFExpr::make(new FExpr_Qcut(cols, nquantiles));
}

DECLARE_PYFN(&pyfn_qcut)
    ->name("qcut")
    ->docs(doc_dt_qcut)
    ->arg_names({"cols", "nquantiles"})
    ->n_positional_args(1)
    ->n_keyword_args(1)
    ->n_required_args(1);




}}  // dt::expr
