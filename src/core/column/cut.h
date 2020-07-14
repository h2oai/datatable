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
#include "column/const.h"
#include "column/virtual.h"
#include "models/utils.h"
#include "stype.h"

namespace dt {


/**
  * Virtual column for data binning.
  */
class Cut_ColumnImpl : public Virtual_ColumnImpl {
  private:
    Column col_;
    double bin_factor_, bin_shift_;

  public:

    template <typename T>
    static ColumnImpl* make(Column&& col, size_t bins) {
      T tmin, tmax;
      bool min_valid = col.stats()->get_stat(Stat::Min, &tmin);
      bool max_valid = col.stats()->get_stat(Stat::Max, &tmax);
      if (!min_valid || !max_valid || _isinf(tmin) || _isinf(tmax) || bins == 0) {
        return new ConstNa_ColumnImpl(col.nrows(), dt::SType::INT32);
      } else {
        double min = static_cast<double>(tmin);
        double max = static_cast<double>(tmax);
        double bin_factor, bin_shift;

        col.cast_inplace(SType::FLOAT64);
        set_cut_coeffs(bin_factor, bin_shift, min, max, bins);

        return new Cut_ColumnImpl(std::move(col), bin_factor, bin_shift);
      }

    }

    Cut_ColumnImpl(Column&& col, double bin_factor, double bin_shift)
      : Virtual_ColumnImpl(col.nrows(), dt::SType::INT32),
        col_(col),
        bin_factor_(bin_factor),
        bin_shift_(bin_shift) {}

    ColumnImpl* clone() const override {
      return new Cut_ColumnImpl(Column(col_), bin_factor_, bin_shift_);
    }

    size_t n_children() const noexcept override {
      return 1;
    }

    const Column& child(size_t i) const override {
      xassert(i == 0);  (void)i;
      return col_;
    }


    bool get_element(size_t i, int32_t* out)  const override {
      double value;
      bool is_valid = col_.get_element(i, &value) && _notnan(bin_factor_);

      *out = static_cast<int32_t>(bin_factor_ * value + bin_shift_);
      return is_valid;
    }


    static void set_cut_coeffs(double& bin_factor, double& bin_shift,
                              const double min, const double max,
                              const size_t bins)
    {
      const double epsilon = static_cast<double>(
                               std::numeric_limits<float>::epsilon()
                             );
      if (min == max) {
        bin_factor = 0;
        bin_shift = 0.5 * (1 - epsilon) * bins;
      } else {
        bin_factor = (1 - epsilon) * bins / (max - min);
        bin_shift = -bin_factor * min;
      }
    }

};



}  // namespace dt
#endif
