//------------------------------------------------------------------------------
// Copyright 2019-2020 H2O.ai
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
#ifndef dt_COLUMN_CUT_h
#define dt_COLUMN_CUT_h
#include <memory>
#include "column/func_unary.h"
#include "column/virtual.h"
#include "stype.h"

namespace dt {


/**
  * Virtual column for data binning.
  */
template <typename T_elems, typename T_stats>
class Cut_ColumnImpl : public Virtual_ColumnImpl {
  private:
    Column col_dbl_;
    double bin_factor_, bin_shift_;

  public:
    Cut_ColumnImpl(Column col, size_t bins)
      : Virtual_ColumnImpl(col.nrows(), col.stype()),
      bin_factor_(dt::NA_F8),
      bin_shift_(dt::NA_F8)
    {
      col_dbl_ = Column(new dt::FuncUnary2_ColumnImpl<T_elems, double>(
                   Column(col),
                   [](T_elems x, bool x_isvalid, double* out) {
                     *out = static_cast<double>(x);
                     return x_isvalid && _isfinite(x);
                   },
                   col.nrows(),
                   dt::SType::FLOAT64
                 ));

      // Get stats
      T_stats tmin, tmax;
      col.stats()->get_stat(Stat::Min, &tmin);
      col.stats()->get_stat(Stat::Max, &tmax);
      double min = static_cast<double>(tmin);
      double max = static_cast<double>(tmax);

      // Set up binning coefficients
      set_cut_coeffs(bin_factor_, bin_shift_, min, max, bins);
    }

    Cut_ColumnImpl(Column col, double bin_factor, double bin_shift)
      : Virtual_ColumnImpl(col.nrows(), col.stype()),
        col_dbl_(col),
        bin_factor_(bin_factor),
        bin_shift_(bin_shift) {}

    ColumnImpl* clone() const override {
      return new Cut_ColumnImpl<T_elems, T_stats>(col_dbl_, bin_factor_, bin_shift_);
    }

    bool allow_parallel_access() const override {
      return col_dbl_.allow_parallel_access();
    }

    size_t n_children() const noexcept override {
      return 1;
    }

    const Column& child(size_t i) const override {
      xassert(i == 0);  (void)i;
      return col_dbl_;
    }


    bool get_element(size_t i, int32_t* out)  const override {
      double value;
      bool is_valid = col_dbl_.get_element(i, &value) && _notnan(bin_factor_);

      if (is_valid) {
        *out = static_cast<int32_t>(bin_factor_ * value + bin_shift_);
      } else {
        return false;
      }

      return true;
    }


    static int set_cut_coeffs(double& bin_factor, double& bin_shift,
                              const double min, const double max,
                              const size_t bins)
    {
      const double epsilon = static_cast<double>(
                               std::numeric_limits<float>::epsilon()
                             );

      if (bins == 0 || !_isfinite(min) || !_isfinite(max)) return -1;

      if (min == max) {
        bin_factor = 0;
        bin_shift = 0.5 * (1 - epsilon) * bins;
      } else {
        bin_factor = (1 - epsilon) * bins / (max - min);
        bin_shift = -bin_factor * min;
      }
      return 0;
    }

};



}  // namespace dt
#endif
