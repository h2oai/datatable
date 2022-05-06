//------------------------------------------------------------------------------
// Copyright 2020-2022 H2O.ai
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
          const Column& coli = wf.get_column(i);
          const dt::Type& typei = coli.type();
          if (!typei.is_numeric_or_void() &&
              !typei.is_boolean() &&
              !typei.is_temporal() &&
              !typei.is_string())
          {
            throw TypeError() << "`qcut()` cannot be applied to "
              << "columns of type: `" << typei << "`";
          }
        }
      }

      // qcut a grouped workframe
      if (ctx.has_groupby()) {
        wf.increase_grouping_mode(Grouping::GtoALL);
        const Groupby& gb = ctx.get_groupby();
        const int32_t* offsets = gb.offsets_r();

        for (size_t i = 0; i < ncols; ++i) {
          colvec colv(gb.size());
          Column coli = wf.retrieve_column(i);
          bool is_grouped = ctx.has_group_column(wf.get_frame_id(i), wf.get_column_id(i));

          for (size_t j = 0; j < gb.size(); ++j) {
            // extact j's group as a column
            Column coli_group = coli;
            size_t group_size = static_cast<size_t>(offsets[j + 1] - offsets[j]);
            RowIndex ri(static_cast<size_t>(offsets[j]), group_size, 1);
            coli_group.apply_rowindex(ri);
            // qcut j's group
            colv[j] = Column(new Latent_ColumnImpl(new Qcut_ColumnImpl(
              std::move(coli_group), nquantiles[i], is_grouped
            )));
          }
          // rbind all the results
          coli = Column::new_na_column(0, SType::VOID);
          coli.rbind(colv, false);
          wf.replace_column(i, std::move(coli));
        }
      } else {
        // qcut ungrouped workframe
        for (size_t i = 0; i < ncols; ++i) {
          Column coli = wf.retrieve_column(i);
          coli = Column(new Latent_ColumnImpl(new Qcut_ColumnImpl(
                   std::move(coli), nquantiles[i]
                 )));
          wf.replace_column(i, std::move(coli));
        }
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
