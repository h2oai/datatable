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
#include "parallel/api.h"
#include "column/const.h"
#include "column/virtual.h"
#include "models/utils.h"
#include "sort.h"
#include "stype.h"
#include "ltype.h"
#include <iostream>

namespace dt {

/**
  *  Virtual column to bin numeric values into equal-population
  *  discrete intervals.
  */
class Qcut_ColumnImpl : public Virtual_ColumnImpl {
  private:
    Column col_;
    double a_, b_;
    int32_t shift_;
    size_t: 32;

  public:
    static Column make(Column&& col, size_t icol, int32_t nquantiles) {
      xassert(nquantiles > 0);

      if (col.ltype() != dt::LType::BOOL && col.ltype() != dt::LType::INT &&
          col.ltype() != dt::LType::REAL && col.ltype() != dt::LType::STRING)
      {
        throw TypeError() << "qcut() can only be applied to numeric and string "
          << "columns, instead column `" << icol << "` has an stype: `"
          << col.stype() << "`";
      }


      auto res = group({col}, {SortFlag::NONE});
      RowIndex ri = std::move(res.first);
      Groupby gb = std::move(res.second);

      // If we only have one group fill it with a constant or NA's.
      if (gb.size() == 1) {

        if (col.get_element_isvalid(0)) {

          return Const_ColumnImpl::make_int_column(
                   col.nrows(),
                   (nquantiles - 1) / 2,
                   SType::INT32
                  );

        } else {

          return Column(new ConstNa_ColumnImpl(
                   col.nrows(),
                   SType::INT32
                 ));

        }
      }

      auto col_out = Column::new_data_column(col.nrows(), SType::INT32);;
      auto data_out = static_cast<int32_t*>(col_out.get_data_editable());
      bool has_nas = false;

      // For NA group, if present, quantiles also become NA.
      {
        size_t row;
        bool row_valid = ri.get_element(0, &row);
        xassert(row_valid); (void)row_valid;
        if (!col.get_element_isvalid(row)) {
          size_t j0, j1;
          gb.get_group(0, &j0, &j1);
          for (size_t j = j0; j < j1; ++j) {
            row_valid = ri.get_element(static_cast<size_t>(j), &row);
            xassert(row_valid); (void)row_valid;
            data_out[row] = GETNA<int32_t>();
          }
          has_nas = true;
        }
      }

      // For non-NA groups set up the actual quantile information.
      constexpr double epsilon = static_cast<double>(
                                   std::numeric_limits<float>::epsilon()
                                 );
      // Number of groups excluding NA group, if it exists.
      size_t ngroups = gb.size() - has_nas;
      double a = nquantiles * (1 - epsilon) / (ngroups - 1);

      dt::parallel_for_dynamic(ngroups,
        [&](size_t i) {
          size_t j0, j1;
          gb.get_group(i + has_nas, &j0, &j1);

          for (size_t j = j0; j < j1; ++j) {
            size_t row;
            bool row_valid = ri.get_element(j, &row);
            xassert(row_valid); (void)row_valid;
            data_out[row] = static_cast<int32_t>(a * i);
          }
      });

      return col_out;
    }

    ColumnImpl* clone() const override {
      return new Qcut_ColumnImpl(Column(col_), a_, b_, shift_);
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


  private:
    Qcut_ColumnImpl(Column&& col, double a, double b, int32_t shift)
      : Virtual_ColumnImpl(col.nrows(), dt::SType::INT32),
        a_(a),
        b_(b),
        shift_(shift)
    {
      xassert(col.stype() == dt::SType::FLOAT64);
      col_ = std::move(col);
    }

};



}  // namespace dt


#endif
