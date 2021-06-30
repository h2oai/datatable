//------------------------------------------------------------------------------
// Copyright 2020-2021 H2O.ai
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
#include "documentation.h"
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
    int32vec nbins_;
    std::vector<std::shared_ptr<dblvec>> bin_edges_;
    bool right_closed_;
    size_t: 56;

  public:
    FExpr_Cut(py::robj arg, int32vec&& nbins,
              std::vector<std::shared_ptr<dblvec>>&& bin_edges,
              bool right_closed
    ) : arg_(as_fexpr(arg)),
        nbins_(std::move(nbins)),
        bin_edges_(std::move(bin_edges)),
        right_closed_(right_closed)
    {}

    std::string repr() const override {
      std::string out = "cut(";
      out += arg_->repr();

      if (nbins_.size()) {
        out += ", nbins=[";
        for (size_t i = 0; i < nbins_.size(); ++i) {
          out += std::to_string(nbins_[i]);
          out += ",";
        }
        out += "]";
      }

      if (bin_edges_.size()) {
        out += ", bins=[";
        for (size_t i = 0; i < bin_edges_.size(); ++i) {
          out += "[";
          for (size_t j = 0; j < bin_edges_[i]->size(); ++j) {
            out += std::to_string((*bin_edges_[i])[j]);
            out += ",";
          }
          out += "],";
        }
        out += "]";
      }
      out += ", right_closed=";
      out += right_closed_? "True" : "False";
      out += ")";
      return out;
    }


    Workframe evaluate_n(EvalContext& ctx) const override {
      if (ctx.has_groupby()) {
        throw NotImplError() << "cut() cannot be used in a groupby context";
      }

      Workframe wf = arg_->evaluate_n(ctx);

      if (bin_edges_.size()) {
        cut_bins(wf);
      } else {
        cut_nbins(wf);
      }

      return wf;
    }


    // Bin data based on the provided number of binning intervals.
    void cut_nbins(Workframe& wf) const {
      const size_t ncols = wf.ncols();

      if (nbins_.size() != ncols && nbins_.size() != 1) {
        throw ValueError() << "When `nbins` has more than one element, its length must be "
          << "the same as the number of columns in the frame/expression, i.e. `"
          << ncols << "`, instead got: `" << nbins_.size() << "`";
      }

      // Bin columns in-place
      for (size_t i = 0; i < ncols; ++i) {
        Column col = wf.retrieve_column(i);

        if (col.ltype() == LType::MU) {
          col = Column(new ConstNa_ColumnImpl(col.nrows(), dt::SType::INT32));
        } else {
          if (!ltype_is_numeric(col.ltype())) {
            throw TypeError() << "cut() can only be applied to numeric "
              << "columns, instead column `" << i << "` has an stype: `"
              << col.stype() << "`";
          }

          // When `nbins_` has only one element we bin all data by using `nbins_[0]`
          size_t nbins_i = i % nbins_.size();
          col = Column(CutNbins_ColumnImpl::make(
                  std::move(col), nbins_[nbins_i], right_closed_
                ));
        }
        wf.replace_column(i, std::move(col));
      }
    }


    // Bin data based on the provided interval edges.
    void cut_bins(Workframe& wf) const {
      const size_t ncols = wf.ncols();

      if (bin_edges_.size() != ncols) {
        throw ValueError() << "Number of elements in `bins` must be equal to "
          << "the number of columns in the frame/expression, i.e. `"
          << ncols << "`, instead got: `" << bin_edges_.size() << "`";
      }

      for (size_t i = 0; i < ncols; ++i) {
        Column col = wf.retrieve_column(i);

        if (col.ltype() == LType::MU) {
          col = Column(new ConstNa_ColumnImpl(col.nrows(), dt::SType::INT32));
        } else {
          if (!ltype_is_numeric(col.ltype())) {

            throw TypeError() << "cut() can only be applied to numeric"
              << "columns, instead column `" << i << "` has an stype: `"
              << col.stype() << "`";

          }
          col.cast_inplace(SType::FLOAT64);

          // Bin column in-place
          col = right_closed_? Column(new CutBins_ColumnImpl<true>(std::move(col), bin_edges_[i]))
                             : Column(new CutBins_ColumnImpl<false>(std::move(col), bin_edges_[i]));
        }
        wf.replace_column(i, std::move(col));
      }
    }


    // Validate bins and convert them to vector
    static std::shared_ptr<dblvec> bins_to_vector(const Column& col, size_t frame_id) {

      auto validate_bin = [&] (bool bin_valid, size_t row) {
        if (!bin_valid) {
          throw ValueError() << "Bin edges must be numeric values only, "
            << "instead for the frame `" << frame_id << "`"
            << " got `None` at row `" << row << "`";
        }
      };

      auto bin_edges_vec = std::make_shared<dblvec>();
      bin_edges_vec->reserve(col.nrows());

      double val;
      bool is_valid = col.get_element(0, &val);
      validate_bin(is_valid, 0);
      bin_edges_vec->push_back(val);

      for (size_t i = 1; i < col.nrows(); ++i) {
        is_valid = col.get_element(i, &val);
        validate_bin(is_valid, i);

        if (val <= (*bin_edges_vec)[i - 1]) {
          throw ValueError() << "Bin edges must be strictly increasing, "
            << "instead for the frame `" << frame_id << "`" << " at rows `"
            << i - 1 << "` " << "and `" << i << "` the values are `"
            << (*bin_edges_vec)[i - 1] << "` and `" << val<< "`";
        }
        (*bin_edges_vec).push_back(val);
      }

      return bin_edges_vec;
    }

};




//------------------------------------------------------------------------------
// Python-facing `cut()` function
//------------------------------------------------------------------------------

static py::oobj pyfn_cut(const py::XArgs& args) {
  auto arg0         = args[0].to_oobj();
  auto nbins_arg    = args[1].to<py::oobj>(py::None());
  auto bins_arg     = args[2].to<py::oobj>(py::None());
  auto right_closed = args[3].to<bool>(true);
  std::vector<std::shared_ptr<dblvec>> bin_edges_vec;
  int32_t nbins_default = 10;
  int32vec nbins_vec;

  if (!bins_arg.is_none() && !nbins_arg.is_none()) {
    throw ValueError() << "`bins` and `nbins` cannot be both set at the same time";
  }

  if (!bins_arg.is_none()) {
    if (!bins_arg.is_list_or_tuple()) {
      throw TypeError() << "`bins` parameter must be a list or a tuple, "
        << "instead got `" << bins_arg.typeobj() << "`";
    }

    size_t i = 0;
    py::oiter bins_iter = bins_arg.to_oiter();

    for (auto bins_frame : bins_iter) {
      DataTable* dt = bins_frame.to_datatable();
      if (dt->ncols() != 1) {
        throw ValueError() << "To bin a column `cut()` needs exactly one column "
          << "with the bin edges, instead for the frame `" << i << "` got: "
          << dt->ncols() << "`";
      }

      if (dt->nrows() < 2) {
        throw ValueError() << "To bin data at least two edges are required, "
          << "instead for the frame `" << i << "` got: `" << dt->nrows() << "`";
      }

      // Validate and materialize bins
      Column col = dt->get_column(0);
      if (!ltype_is_numeric(col.ltype())) {
        throw TypeError() << "Bin edges must be provided as the numeric columns only, "
          << "instead for the frame `" << i << "` the column stype is `"
          << col.stype() << "`";
      }
      col.cast_inplace(SType::FLOAT64);
      bin_edges_vec.push_back(FExpr_Cut::bins_to_vector(col, i));
      i++;
    }
  } else {
    bool nbins_list_or_tuple = nbins_arg.is_list_or_tuple();

    if (nbins_list_or_tuple) {
      py::oiter nbins_iter = nbins_arg.to_oiter();
      nbins_vec.reserve(nbins_iter.size());
      size_t i = 0;
      for (auto nbins_val : nbins_iter) {
        int32_t nbins = nbins_val.to_int32_strict();
        if (nbins <= 0) {
          throw ValueError() << "All elements in `nbins` must be positive, "
            << "got `nbins[" << i << "`]: `" << nbins << "`";
        }

        nbins_vec.push_back(nbins);
        i++;
      }

    } else {
      if (!nbins_arg.is_none()) {
        nbins_default = nbins_arg.to_int32_strict();
        if (nbins_default <= 0) {
          throw ValueError() << "Number of bins must be positive, instead got: `"
            << nbins_default << "`";
        }
      }
      nbins_vec.push_back(nbins_default);
    }
  }

  return PyFExpr::make(new FExpr_Cut(arg0, std::move(nbins_vec), std::move(bin_edges_vec), right_closed));
}

DECLARE_PYFN(&pyfn_cut)
    ->name("cut")
    ->docs(doc_dt_cut)
    ->arg_names({"cols", "nbins", "bins", "right_closed"})
    ->n_positional_args(1)
    ->n_keyword_args(3)
    ->n_required_args(1);




}}  // dt::expr
