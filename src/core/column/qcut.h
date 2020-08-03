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
    int32_t nquantiles_;
    size_t : 32;

  public:
    Qcut_ColumnImpl(Column&& col, int32_t nquantiles)
      : Virtual_ColumnImpl(col.nrows(), dt::SType::INT32),
        nquantiles_(nquantiles)
    {
      xassert(nquantiles > 0);
      xassert(col.ltype() == dt::LType::BOOL || col.ltype() == dt::LType::INT ||
              col.ltype() == dt::LType::REAL || col.ltype() == dt::LType::STRING);
      col_ = std::move(col);
    }


    void materialize(Column& col_out, bool) override {
      auto res = group({col_}, {SortFlag::NONE});
      RowIndex ri = std::move(res.first);
      Groupby gb = std::move(res.second);

      // If we have one group only, fill it with constants or NA's.
      if (gb.size() == 1) {
        if (col_.get_element_isvalid(0)) {

          col_out = Column(Const_ColumnImpl::make_int_column(
                      col_.nrows(),
                      (nquantiles_ - 1) / 2,
                      SType::INT32
                    ));

        } else {

          col_out = Column(new ConstNa_ColumnImpl(
                      col_.nrows(),
                      SType::INT32
                    ));

        }
        return;
      }

      col_out = Column::new_data_column(col_.nrows(), SType::INT32);;
      auto data_out = static_cast<int32_t*>(col_out.get_data_editable());
      bool has_nas = false;

      // For NA group, if present, quantiles also become NA's.
      {
        size_t row;
        bool row_valid = ri.get_element(0, &row);
        xassert(row_valid); (void)row_valid;
        if (!col_.get_element_isvalid(row)) {
          has_nas = true;
          size_t j0, j1;
          gb.get_group(0, &j0, &j1);
          xassert(j0 == 0); (void) j0;

          dt::parallel_for_static(j1,
            [&](size_t j) {
              row_valid = ri.get_element(j, &row);
              xassert(row_valid); (void)row_valid;
              data_out[row] = GETNA<int32_t>();
            }
          );
        }
      }

      // For all the other groups set up the actual quantiles.
      constexpr double epsilon = static_cast<double>(
                                   std::numeric_limits<float>::epsilon()
                                 );

      // Number of groups excluding NA group, if it exists.
      size_t ngroups = gb.size() - has_nas;
      double a = nquantiles_ * (1 - epsilon) / (ngroups - 1);

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
    }


    ColumnImpl* clone() const override {
      return new Qcut_ColumnImpl(Column(col_), nquantiles_);
    }


    size_t n_children() const noexcept override {
      return 1;
    }


    const Column& child(size_t i) const override {
      xassert(i == 0);  (void)i;
      return col_;
    }

};



}  // namespace dt


#endif
