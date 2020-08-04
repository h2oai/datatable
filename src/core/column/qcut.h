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

namespace dt {

/**
  *  Virtual column to bin numeric values into equal-population
  *  discrete intervals.
  */
class Qcut_ColumnImpl : public Virtual_ColumnImpl {
  private:
    Column col_;
    RowIndex ri_;
    Groupby gb_;
    int32_t nquantiles_;
    size_t : 32;

  public:
    static ColumnImpl* make(Column&& col, int32_t nquantiles) {
      xassert(nquantiles > 0);
      auto res = group({col}, {SortFlag::NONE});
      RowIndex ri = std::move(res.first);
      Groupby gb = std::move(res.second);

      // If there is one group only, fill it with constants or NA's.
      if (gb.size() == 1) {
        if (col.get_element_isvalid(0)) {
          return new ConstInt_ColumnImpl(
                   col.nrows(),
                   (nquantiles - 1) / 2,
                   SType::INT32
                 );
        } else {
          return new ConstNa_ColumnImpl(col.nrows(), SType::INT32);
        }
      }

      return new Latent_ColumnImpl(new Qcut_ColumnImpl(
                   std::move(col), nquantiles, std::move(ri), std::move(gb)
                 ));

    }

    void materialize(Column& col_out, bool) override {
      Column col_out_ = Column::new_data_column(col_.nrows(), SType::INT32);
      auto data_out_ = static_cast<int32_t*>(col_out_.get_data_editable());

      constexpr double epsilon = static_cast<double>(
                                   std::numeric_limits<float>::epsilon()
                                 );

      // Check if there is an NA group.
      bool has_na_group;
      {
        size_t row;
        bool row_valid = ri_.get_element(0, &row);
        xassert(row_valid); (void) row_valid;
        has_na_group = !col_.get_element_isvalid(row);
      }

      // Set up number of valid groups and the quantile coefficient.
      size_t ngroups = gb_.size() - has_na_group;
      double a = nquantiles_ * (1 - epsilon) / (ngroups - 1);

      // Set up the actual quantiles.
      dt::parallel_for_dynamic(gb_.size(),
        [&](size_t i) {
          bool is_na_group = (i == 0 && has_na_group);
          size_t j0, j1;
          auto q = is_na_group? GETNA<int32_t>()
                              : static_cast<int32_t>(a * (i - has_na_group));

          gb_.get_group(i, &j0, &j1);

          for (size_t j = j0; j < j1; ++j) {
            size_t row;
            bool row_valid = ri_.get_element(j, &row);
            xassert(row_valid); (void) row_valid;
            data_out_[row] = q;
          }
      });

      col_out = std::move(col_out_);
    }


    ColumnImpl* clone() const override {
      return new Qcut_ColumnImpl(
                   Column(col_), nquantiles_, RowIndex(ri_), Groupby(gb_)
                 );
    }


    size_t n_children() const noexcept override {
      return 1;
    }


    const Column& child(size_t i) const override {
      xassert(i == 0);  (void)i;
      return col_;
    }

  private:
    Qcut_ColumnImpl(Column&& col, int32_t nquantiles, RowIndex&& ri, Groupby&& gb)
      : Virtual_ColumnImpl(col.nrows(), dt::SType::INT32),
        nquantiles_(nquantiles)
    {
      xassert(nquantiles > 0);
      xassert(ri.size() == col.nrows());
      xassert(gb.last_offset() == col.nrows());
      xassert(col.ltype() == dt::LType::BOOL ||
              col.ltype() == dt::LType::INT ||
              col.ltype() == dt::LType::REAL);
      col_ = std::move(col);
      ri_ = std::move(ri);
      gb_ = std::move(gb);
    }

};



}  // namespace dt


#endif
