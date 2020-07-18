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
 *  Virtual column to bin numeric values into discrete intervals.
 *
 *  The binning method consists of the following steps
 *  1) we calculate min/max for the input column, and if one of these is NaN
 *     or inf, or `bins == 0` the `ConstNa_ColumnImpl` is returned.
 *  2) for valid and finite min/max we normalize column data to
 *       [0; 1 - epsilon] range for `right == true`
 *     or to
 *       [epsilon - 1; 0] range for `right == false`.
 *     Then, we multiply the normalized values by the number of the requested
 *     bins and add a proper shift to compute the final bin numbers.
 *
 *  Step #2 is implemented by employing the following formula
 *    bin_i = static_cast<int32_t>(x_i * bin_a + bin_b) + bin_shift
 *
 *  If max == min, all values end up in the central bin determined
 *  based on the `right` parameter, i.e.
 *    bin_a = 0;
 *    bin_b = (1 + (1 - 2 * right) * epsilon) * bins / 2;
 *    bin_shift = 0
 *
 *  When `min != max`, and `right == true`, we set
 *    bin_a = bins / (max + epsilon - min)
 *    bin_b  = -bin_a * min
 *    bin_shift = 0
 *  simply scaling data to [0; 1 - epsilon] and then multiplying
 *  them by `bins` number.
 *
 *  When `min != max`, and `right == false`, we set
 *    bin_a = bins / (max + epsilon - min)
 *    bin_b  = -bin_a * min + (epsilon - 1) * bins
 *    bin_shift = bins - 1;
 *  scaling data to [epsilon - 1; 0], multiplying them by `bins`,
 *  and then shifting the resulting values by `bins - 1`. The last shift is
 *  needed to convert auxiliary negative bins to the corresponding
 *  positive bins.
 */
class Cut_ColumnImpl : public Virtual_ColumnImpl {
  private:
    Column col_;
    double bin_a_, bin_b_;
    int32_t bin_shift_;
    size_t: 32;

  public:

    template <typename T>
    static ColumnImpl* make(Column&& col, size_t bins, bool right) {
      T tmin, tmax;
      bool min_valid = col.stats()->get_stat(Stat::Min, &tmin);
      bool max_valid = col.stats()->get_stat(Stat::Max, &tmax);
      if (!min_valid || !max_valid || _isinf(tmin) || _isinf(tmax) || bins == 0) {
        return new ConstNa_ColumnImpl(col.nrows(), dt::SType::INT32);
      } else {
        double min = static_cast<double>(tmin);
        double max = static_cast<double>(tmax);
        double bin_a, bin_b;
        int32_t bin_shift;

        col.cast_inplace(SType::FLOAT64);
        set_cut_coeffs(bin_a, bin_b, bin_shift, min, max, bins, right);

        return new Cut_ColumnImpl(std::move(col), bin_a, bin_b, bin_shift);
      }

    }

    Cut_ColumnImpl(Column&& col, double bin_a, double bin_b, int32_t bin_shift)
      : Virtual_ColumnImpl(col.nrows(), dt::SType::INT32),
        col_(col),
        bin_a_(bin_a),
        bin_b_(bin_b),
        bin_shift_(bin_shift) {}

    ColumnImpl* clone() const override {
      return new Cut_ColumnImpl(Column(col_), bin_a_, bin_b_, bin_shift_);
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
      bool is_valid = col_.get_element(i, &value);
      *out = static_cast<int32_t>(bin_a_ * value + bin_b_) + bin_shift_;
      return is_valid;
    }


    static void set_cut_coeffs(double& bin_a, double& bin_b, int32_t& bin_shift,
                               double min, double max,
                               size_t bins, const bool right)
    {
      const double epsilon = static_cast<double>(
                               std::numeric_limits<float>::epsilon()
                             );

      bin_shift = 0;

      if (min == max) {
        bin_a = 0;
        bin_b = (1 + (1 - 2 * right) * epsilon) * bins / 2;
      } else {
        max += epsilon;
        bin_a = bins / (max - min);
        bin_b = -bin_a * min;
        if (!right) {
          bin_b += (epsilon - 1) * bins;
          bin_shift = static_cast<int32_t>(bins) - 1;
        }
      }
    }

};



}  // namespace dt


#endif
