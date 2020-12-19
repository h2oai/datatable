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
#include "column/cut.h"
#include "expr/eval_context.h"
#include "expr/fexpr_func.h"
#include "frame/py_frame.h"
#include "python/xargs.h"


namespace dt {
namespace expr {



//------------------------------------------------------------------------------
// FExpr_Cut
//------------------------------------------------------------------------------

class FExpr_Cut : public FExpr_Func {
  private:
    ptrExpr arg_;
    py::oobj py_nbins_;
    py::oobj py_bins_;
    bool right_closed_;
    size_t: 56;

  public:
    FExpr_Cut(py::robj arg, py::robj py_nbins, py::robj py_bins, bool right_closed)
      : arg_(as_fexpr(arg)),
        py_nbins_(py_nbins),
        py_bins_(py_bins),
        right_closed_(right_closed)
    {}

    std::string repr() const override {
      std::string out = "cut(";
      out += arg_->repr();
      if (!py_nbins_.is_none()) {
        out += ", nbins=";
        out += py_nbins_.repr().to_string();
        out += ", bins=";
        out += py_bins_.repr().to_string();
        out += ", right_closed=";
        out += right_closed_? "True" : "False";
      }
      out += ")";
      return out;
    }


    Workframe evaluate_n(EvalContext& ctx) const override {
      if (ctx.has_groupby()) {
        throw NotImplError() << "cut() cannot be used in a groupby context";
      }

      Workframe wf = arg_->evaluate_n(ctx);

      bool defined_bins = !py_bins_.is_none();
      bool defined_nbins = !py_nbins_.is_none();

      if (defined_bins && defined_nbins) {
        throw ValueError() << "`bins` and `nbins` cannot be both set at the same time";
      }

      if (defined_bins) {
        cut_bins(wf);
      } else {
        cut_nbins(wf);
      }

      return wf;
    }


    // Bin data based on the provided number of binning intervals.
    void cut_nbins(Workframe& wf) const {
      int32_t nbins_default = 10;
      const size_t ncols = wf.ncols();

      int32vec nbins(ncols);
      bool nbins_list_or_tuple = py_nbins_.is_list_or_tuple();

      if (nbins_list_or_tuple) {
        py::oiter py_nbins = py_nbins_.to_oiter();
        if (py_nbins.size() != ncols) {
          throw ValueError() << "When `nbins` is a list or a tuple, its length must be "
            << "the same as the number of columns in the frame/expression, i.e. `"
            << ncols << "`, instead got: `" << py_nbins.size() << "`";
        }

        size_t i = 0;
        for (auto py_nbin : py_nbins) {
          int32_t nbin = py_nbin.to_int32_strict();
          if (nbin <= 0) {
            throw ValueError() << "All elements in `nbins` must be positive, "
              << "got `nbins[" << i << "`]: `" << nbin << "`";
          }

          nbins[i++] = nbin;
        }
        xassert(i == ncols);

      } else {
        if (!py_nbins_.is_none()) {
          nbins_default = py_nbins_.to_int32_strict();
          if (nbins_default <= 0) {
            throw ValueError() << "Number of bins must be positive, instead got: `"
              << nbins_default << "`";
          }
        }

        for (size_t i = 0; i < ncols; ++i) {
          nbins[i] = nbins_default;
        }
      }

      // Bin columns in-place
      for (size_t i = 0; i < ncols; ++i) {
        Column col = wf.retrieve_column(i);

        if (col.ltype() == LType::MU) {
          col = Column(new ConstNa_ColumnImpl(col.nrows(), dt::SType::INT32));
        } else {
          if (!ltype_is_numeric(col.ltype())) {
            throw TypeError() << "cut() can only be applied to numeric or void "
              << "columns, instead column `" << i << "` has an stype: `"
              << col.stype() << "`";
          }

          col = Column(CutNbins_ColumnImpl::make(
                  std::move(col), nbins[i], right_closed_
                ));
        }
        wf.replace_column(i, std::move(col));
      }
    }


    // Bin data based on the provided interval edges.
    void cut_bins(Workframe& wf) const {
      const size_t ncols = wf.ncols();

      if (!py_bins_.is_list_or_tuple()) {
        throw TypeError() << "`bins` parameter must be a list or a tuple, "
          << "instead got `" << py_bins_.typeobj() << "`";
      }

      py::oiter py_bins = py_bins_.to_oiter();

      if (py_bins.size() != ncols) {
        throw ValueError() << "Number of elements in `bins` must be equal to "
          << "the number of columns in the frame/expression, i.e. `"
          << ncols << "`, instead got: `" << py_bins.size() << "`";
      }

      size_t i = 0;
      for (auto py_bin : py_bins) {
        DataTable* dt = py_bin.to_datatable();
        if (dt->ncols() != 1) {
          throw ValueError() << "To bin a column `cut()` needs exactly one column "
            << "with the bin edges, instead for the frame `" << i << "` got: "
            << dt->ncols() << "`";
        }

        if (dt->nrows() < 2) {
          throw ValueError() << "To bin data at least two edges are required, "
            << "instead for the frame `" << i << "` got: `" << dt->nrows() << "`";
        }

        // Retrieve bins
        Column bins = dt->get_column(0);
        if (!ltype_is_numeric(bins.ltype())) {
          throw TypeError() << "Bin edges must be provided as the numeric columns only, "
            << "instead for the frame `" << i << "` the column stype is `"
            << bins.stype() << "`";
        }
        bins.cast_inplace(SType::FLOAT64);
        validate_bins(bins, i);
        bins.materialize();

        // Retrieve actual data
        Column col = wf.retrieve_column(i);

        if (col.ltype() == LType::MU) {
          col = Column(new ConstNa_ColumnImpl(col.nrows(), dt::SType::INT32));
        } else {
          if (!ltype_is_numeric(col.ltype())) {
            throw TypeError() << "cut() can only be applied to numeric or void "
              << "columns, instead column `" << i << "` has an stype: `"
              << col.stype() << "`";

          }
          col.cast_inplace(SType::FLOAT64);

          // Bin column in-place
          col = right_closed_? Column(new CutBins_ColumnImpl<true>(std::move(col), std::move(bins)))
                             : Column(new CutBins_ColumnImpl<false>(std::move(col), std::move(bins)));
        }
        wf.replace_column(i, std::move(col));
        i++;
      }
    }


    // Ensure that interval edges are numeric values and are strictly increasing.
    void validate_bins(const Column& bins, size_t frame_id) const {
      double val1;
      bool is_valid = bins.get_element(0, &val1);

      if (!is_valid) {
        throw ValueError() << "Bin edges must be numeric values only, "
          << "instead for the frame `" << frame_id << "`" << " got `None` at row `0`";
      }

      for (size_t i = 1; i < bins.nrows(); ++i) {
        double val2;
        is_valid = bins.get_element(i, &val2);
        if (!is_valid) {
          throw ValueError() << "Bin edges must be numeric values only, "
            << "instead for the frame `" << frame_id << "`" << " got `None` at row `" << i << "`";
        }
        if (val2 <= val1) {
          throw ValueError() << "Bin edges must be strictly increasing, "
            << "instead for the frame `" << frame_id << "`" << " at rows `" << i - 1 << "` "
            << "and `" << i << "` the values are `" << val1 << "` and `" << val2<< "`";
        }
        val1 = val2;
      }
    }

};




//------------------------------------------------------------------------------
// Python-facing `cut()` function
//------------------------------------------------------------------------------

static const char* doc_cut =
R"(cut(cols, nbins=10, right_closed=True)
--
.. x-version-added:: 0.11

Cut all the columns from `cols` by binning their values into
equal-width discrete intervals.

Parameters
----------
cols: FExpr
    Input data for equal-width interval binning.

nbins: int | List[int]
    When a single number is specified, this number of bins
    will be used to bin each column from `cols`.
    When a list or a tuple is provided, each column will be binned
    by using its own number of bins. In the latter case,
    the list/tuple length must be equal to the number of columns
    in `cols`.

bins: List[Frame]
    List/tuple of single-column frames containing interval edges
    in strictly increasing order, that will be used for binning
    of the corresponding columns from `cols`. The list/tuple
    length must be equal to the number of columns in `cols`.

right_closed: bool
    Each binning interval is `half-open`_. This flag indicates whether
    the right edge of the interval is closed, or not.

return: FExpr
    f-expression that converts input columns into the columns filled
    with the respective bin ids.

See also
--------
:func:`qcut()` -- function for equal-population binning.

.. _`half-open`: https://en.wikipedia.org/wiki/Interval_(mathematics)#Terminology

)";

static py::oobj pyfn_cut(const py::XArgs& args) {
  auto arg0         = args[0].to_oobj();
  auto nbins        = args[1].to<py::oobj>(py::None());
  auto bins         = args[2].to<py::oobj>(py::None());
  auto right_closed = args[3].to<bool>(true);
  return PyFExpr::make(new FExpr_Cut(arg0, nbins, bins, right_closed));
}

DECLARE_PYFN(&pyfn_cut)
    ->name("cut")
    ->docs(doc_cut)
    ->arg_names({"cols", "nbins", "bins", "right_closed"})
    ->n_positional_args(1)
    ->n_keyword_args(3)
    ->n_required_args(1);


}}  // dt::expr
