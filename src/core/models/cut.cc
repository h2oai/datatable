//------------------------------------------------------------------------------
// Copyright 2018-2020 H2O.ai
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
#include "column/func_unary.h"
#include "datatablemodule.h"
#include "datatable.h"
#include "frame/py_frame.h"
#include "ltype.h"
#include "models/cut.h"
#include "models/py_validator.h"
#include "stype.h"
#include "parallel/api.h"
#include "python/obj.h"


namespace py {


static PKArgs args_cut(
  1, 1, 0, false, false,
  {
    "frame", "bins"
  },
  "cut",

R"(cut(frame, bins=10)
--

Bin all the columns in a frame.

Parameters
----------
frame: Frame
    Frame, where each column must be of a numeric type.
bins: int | list or a tuple of int
    Number of bins to be used the frame, or a list/tuple
    that contains numbers of bins for the corresponding columns.
    In the latter case, the list/tuple length must be equal
    to the number of columns in the frame.

Returns
-------
Frame, where each column consists of the respective bin ids.
)"
);


/**
 *  Main Python cut function.
 */
static oobj cut(const PKArgs& args) {
  const Arg& arg_frame = args[0];
  const Arg& arg_bins  = args[1];

  if (arg_frame.is_undefined()) {
    throw ValueError() << "Required parameter `frame` is missing";
  }

  if (arg_frame.is_none()) {
    return py::None();
  }

  DataTable* dt_in = arg_frame.to_datatable();


  for (size_t i = 0; i < dt_in->ncols(); ++i) {
    const Column& col = dt_in->get_column(i);
    if (col.ltype() != dt::LType::BOOL &&
        col.ltype() != dt::LType::INT &&
        col.ltype() != dt::LType::REAL)
    {

      throw TypeError() << "All frame columns must be numeric, "
        "instead column `" << i << "` has stype `" << col.stype() << "`";

    }
  }

  sztvec bins(dt_in->ncols());
  size_t bins_default = 10;
  bool defined_bins = !arg_bins.is_none_or_undefined();
  bool bins_list_or_tuple = arg_bins.is_list_or_tuple();

  if (bins_list_or_tuple) {
    py::oiter py_bins = arg_bins.to_oiter();
    if (py_bins.size() != dt_in->ncols()) {
      throw ValueError() << "When `bins` is a list or a tuple, its length must be "
        << "the same as the number of columns in the frame, i.e. `"
        << dt_in->ncols() << "`, instead got: `" << py_bins.size() << "`";

    }

    size_t i = 0;
    for (auto py_bin : py_bins) {
      size_t bin = py_bin.to_size_t();
      py::Validator::check_positive(bin, arg_bins);
      bins[i++] = bin;
    }
    xassert(i == dt_in->ncols());

  } else {
    if (defined_bins) bins_default = arg_bins.to_size_t();
    py::Validator::check_positive(bins_default, arg_bins);
    for (size_t i = 0; i < dt_in->ncols(); ++i) {
      bins[i] = bins_default;
    }
  }

  dtptr dt_bins = cut(dt_in, bins);
  py::oobj py_bins = py::Frame::oframe(dt_bins.release());

  return py_bins;
}



void DatatableModule::init_methods_cut() {
  ADD_FN(&py::cut, py::args_cut);
}

}  // namespace py


/**
 *  Cut a datatable.
 */
dtptr cut(DataTable* dt_in, sztvec bins) {
  xassert(dt_in->ncols() == bins.size());

  const size_t ncols = dt_in->ncols();
  colvec outcols;
  outcols.reserve(ncols);

  for (size_t i = 0; i < ncols; ++i) {
    const Column& col = dt_in->get_column(i);
    Column col_cut;

    switch (col.stype()) {
      case dt::SType::BOOL:    col_cut = cut_column<int8_t, int64_t>(col, bins[i]);;
                               break;
      case dt::SType::INT8:    col_cut = cut_column<int8_t, int64_t>(col, bins[i]);
                               break;
      case dt::SType::INT16:   col_cut = cut_column<int16_t, int64_t>(col, bins[i]);
                               break;
      case dt::SType::INT32:   col_cut = cut_column<int32_t, int64_t>(col, bins[i]);
                               break;
      case dt::SType::INT64:   col_cut = cut_column<int64_t, int64_t>(col, bins[i]);
                               break;
      case dt::SType::FLOAT32: col_cut = cut_column<float, double>(col, bins[i]);
                               break;
      case dt::SType::FLOAT64: col_cut = cut_column<double, double>(col, bins[i]);
                               break;
      default:                 throw ValueError() << "Columns with stype `" << col.stype()
                                 << "` are not supported";
    }
    outcols.push_back(col_cut);
  }

  auto dt_bins = dtptr(new DataTable(std::move(outcols), *dt_in));
  return dt_bins;
}


/**
 *  Cut a single column.
 */
template <typename T_elems, typename T_stats>
Column cut_column(const Column& col_, size_t bins) {
  // Virtual column to cast data to double
  Column col_dbl = Column(new dt::FuncUnary2_ColumnImpl<T_elems, double>(
                     Column(col_),
                     [](T_elems x, bool x_isvalid, double* out) {
                       *out = static_cast<double>(x);
                       return x_isvalid && _isfinite(x);
                     },
                     col_.nrows(),
                     dt::SType::FLOAT64
                   ));

  // Output column
  Column col_cut = Column::new_data_column(col_.nrows(), dt::SType::INT32);
  auto col_cut_data = static_cast<int32_t*>(col_cut.get_data_editable());

  // Get stats
  T_stats tmin, tmax;
  col_.stats()->get_stat(Stat::Min, &tmin);
  col_.stats()->get_stat(Stat::Max, &tmax);
  double min = static_cast<double>(tmin);
  double max = static_cast<double>(tmax);

  // Set up binning coefficients
  double epsilon = static_cast<double>(std::numeric_limits<float>::epsilon());
  double norm_factor, norm_shift;
  if (tmin == tmax) {
    norm_factor = 0;
    norm_shift = 0.5 * (1 - epsilon) * bins;
  } else {
    norm_factor = (1 - epsilon) * bins / (max - min);
    norm_shift = -norm_factor * min;
  }

  // Do actual binning
  dt::parallel_for_static(col_dbl.nrows(),
    [&](size_t i) {
      double value;
      bool is_valid = col_dbl.get_element(i, &value);
      if (is_valid) {
        col_cut_data[i] = static_cast<int32_t>(norm_factor * value + norm_shift);
      } else {
        col_cut_data[i] = dt::GETNA<int32_t>();
      }
    });

  return col_cut;
}


