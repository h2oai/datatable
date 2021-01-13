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
#ifndef dt_COLUMN_CUT_h
#define dt_COLUMN_CUT_h
#include <memory>
#include "column/const.h"
#include "column/virtual.h"
#include "models/utils.h"
#include "stype.h"
#include "ltype.h"


namespace dt {

/**
  *  Virtual column to bin numeric values into equal-width discrete intervals.
  *
  *  The binning method consists of the following steps
  *
  *    1) calculate min/max for the input column, and if one of these is NaN
  *       or inf, or `nbins == 0` return the `ConstNa_ColumnImpl`.
  *
  *    2) for valid and finite min/max normalize column data to
  *
  *         - [0; 1 - epsilon] interval, if right-closed bins are requested, or to
  *         - [epsilon - 1; 0] interval, otherwise.
  *
  *       Then, multiply the normalized values by the number of the requested
  *       bins and add a proper shift to compute the final bin ids. The following
  *       formula is used for that (note, casting to an integer will truncate
  *       toward zero)
  *
  *         bin_id_i = static_cast<int32_t>(x_i * a + b) + shift
  *
  *       2.1) if `max == min`, all values end up in the central bin which id
  *            is determined based on the `right_closed` parameter being
  *            `true` or `false`, i.e.
  *
  *              a = 0;
  *              b = (nbins - right_closed) / 2;
  *              shift = 0
  *
  *       2.2) when `min != max`, and `right_closed == true`, set
  *
  *              a = (1 - epsilon) * nbins / (max - min)
  *              b = -a * min
  *              shift = 0
  *
  *            scaling data to [0; 1 - epsilon] and then multiplying them
  *            by `nbins`.
  *
  *       2.3) When `min != max`, and `right_closed == false`, set
  *
  *              a = (1 - epsilon) * nbins / (max - min)
  *              b = -a * max
  *              shift = nbins - 1;
  *
  *            scaling data to [epsilon - 1; 0], multiplying them by `nbins`,
  *            and then shifting the resulting values by `nbins - 1`. The final
  *            shift converts the auxiliary negative bin ids to the corresponding
  *            positive bin ids.
  *
  */
class CutNbins_ColumnImpl : public Virtual_ColumnImpl {
  private:
    Column col_;
    double a_, b_;
    int32_t shift_;
    size_t: 32;

  public:
    static ColumnImpl* make(Column&& col, int32_t nbins, bool right_closed) {
      xassert(nbins > 0);
      xassert(ltype_is_numeric(col.ltype()));

      bool min_valid, max_valid;
      double min, max;

      switch (col.ltype()) {
        case dt::LType::BOOL:
        case dt::LType::INT:   {
                                 int64_t min_int, max_int;
                                 min_valid = col.stats()->get_stat(Stat::Min, &min_int);
                                 max_valid = col.stats()->get_stat(Stat::Max, &max_int);
                                 min = static_cast<double>(min_int);
                                 max = static_cast<double>(max_int);
                               }
                               break;

        case dt::LType::REAL:  {
                                 min_valid = col.stats()->get_stat(Stat::Min, &min);
                                 max_valid = col.stats()->get_stat(Stat::Max, &max);

                               }
                               break;

        default:  throw TypeError() << "cut() can only be applied to numeric or void "
                    << "columns, instead got an stype: `" << col.stype() << "`";
      }


      if (!min_valid || !max_valid || _isinf(min) || _isinf(max)) {
        return new ConstNa_ColumnImpl(col.nrows(), dt::SType::INT32);

      } else {
        double a, b;
        int32_t shift;

        col.cast_inplace(SType::FLOAT64);
        compute_cut_coeffs(a, b, shift, min, max, nbins, right_closed);

        return new CutNbins_ColumnImpl(std::move(col), a, b, shift);
      }
    }

    ColumnImpl* clone() const override {
      return new CutNbins_ColumnImpl(Column(col_), a_, b_, shift_);
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
      *out = static_cast<int32_t>(a_ * value + b_) + shift_;
      return is_valid;
    }


    static void compute_cut_coeffs(double& a, double& b, int32_t& shift,
                                   const double min, const double max,
                                   const int32_t nbins, const bool right_closed)
    {
      shift = 0;

      if (min == max) {
        a = 0;
        b = (nbins - right_closed) / 2;
      } else {
        // Reasonably small epsilon for scaling, note that
        // std::numeric_limits<double>::epsilon() is too small and will
        // have no effect for some data
        constexpr double epsilon = static_cast<double>(
                                     std::numeric_limits<float>::epsilon()
                                   );

        a = (1 - epsilon) * nbins / (max - min);
        b = -a * min;

        if (!right_closed) {
          b = -a * max;
          shift = nbins - 1;
        }
      }
    }


  private:
    CutNbins_ColumnImpl(Column&& col, double a, double b, int32_t shift)
      : Virtual_ColumnImpl(col.nrows(), dt::SType::INT32),
        a_(a),
        b_(b),
        shift_(shift)
    {
      xassert(col.stype() == dt::SType::FLOAT64);
      col_ = std::move(col);
    }

};


template <bool RIGHT_CLOSED>
class CutBins_ColumnImpl : public Virtual_ColumnImpl {
  private:
    Column col_;
    std::shared_ptr<dblvec> bin_edges_;

  public:

    CutBins_ColumnImpl(Column&& col, std::shared_ptr<dblvec> bin_edges)
      : Virtual_ColumnImpl(col.nrows(), dt::SType::INT32),
        col_(std::move(col)),
        bin_edges_(bin_edges)
    {
      xassert(ltype_is_numeric(col_.ltype()));
      xassert(bin_edges_->size() >= 2);
    }

    ColumnImpl* clone() const override {
      return new CutBins_ColumnImpl<RIGHT_CLOSED>(Column(col_), bin_edges_);
    }

    size_t n_children() const noexcept override {
      return 1;
    }

    const Column& child(size_t i) const override {
      xassert(i == 0);  (void)i;
      return col_;
    }

    inline bool gt(double v1, double v2) const {
      if (RIGHT_CLOSED) {
        return v1 > v2;
      } else {
        return v1 >= v2;
      }
    }

    inline bool lt(double v1, double v2) const {
      if (RIGHT_CLOSED) {
        return v1 <= v2;
      } else {
        return v1 < v2;
      }
    }


    bool get_element(size_t i, int32_t* out) const override {
      double value;
      bool is_valid = col_.get_element(i, &value);

      if (is_valid) {
        size_t nbin_edges = bin_edges_->size();
        is_valid = false;

        if (gt(value, (*bin_edges_)[0]) && lt(value, (*bin_edges_)[nbin_edges - 1])) {
          is_valid = true;
          *out = static_cast<int32_t>(bin_value(value, 0, nbin_edges - 1));
        }

      }
      return is_valid;
    }


    size_t bin_value(double value, size_t left, size_t right) const {
      xassert(right > left);

      if (right == left + 1) {
        return left;
      }

      size_t middle = (left + right) / 2;

      if (gt(value, (*bin_edges_)[middle])) {
        return bin_value(value, middle, right);
      } else {
        return bin_value(value, left, middle);
      }

    }

};


}  // namespace dt


#endif
