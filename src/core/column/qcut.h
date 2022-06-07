//------------------------------------------------------------------------------
// Copyright 2020-2022 H2O.ai
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
#include "ltype.h"
#include "models/utils.h"
#include "parallel/api.h"
#include "sort.h"
namespace dt {


/**
  *  Virtual column to bin input data into intervals with approximately
  *  equal populations. If there are duplicate values in the
  *  data, they will all be placed into the same bin. In extreme cases
  *  this may cause the bins to be highly unbalanced.
  *
  *  Quantiles are generated based on the element/group information
  *  obtained from the groupby operation, i.e. rowindex and offsets.
  *  These groups, having ids 0, 1, ..., ngroups - 1, are binned
  *  into `nquantiles` equal-width discrete intervals. As a result,
  *  all the duplicates of a value `x` will go to the same `x_q` quantile.
  *
  *  `Qcut_ColumnImpl` class is designed to be wrapped with
  *  the `Latent_ColumnImpl` that will invoke `Qcut_ColumnImpl::materialize()`
  *  on the first access to the data, because it is much more
  *  efficient to generate all the quantiles at once in parallel.
  *
  *  In the case, when all the values are missing or constant,
  *  e.g. the input column is already grouped,
  *  `materialize()` will fallback to the corresponding virtual
  *  column implementation: `ConstNa_ColumnImpl` or `ConstInt_ColumnImpl`,
  *  that gets materialized upon return. The last step is a temporary
  *  workaround since `Latent_ColumnImpl` doesn't support `materialize()`
  *  returning virtual columns.
  *
  */
class Qcut_ColumnImpl : public Virtual_ColumnImpl {
  private:
    Column col_;
    int32_t nquantiles_;
    bool is_const_;
    size_t : 24;

  public:
    Qcut_ColumnImpl(Column&& col, int32_t nquantiles, bool is_const = false)
      : Virtual_ColumnImpl(col.nrows(), dt::SType::INT32),
        nquantiles_(nquantiles),
        is_const_(is_const)
    {
      xassert(nquantiles > 0);
      xassert(col.ltype() != dt::LType::OBJECT);
      col_ = std::move(col);
    }

    void materialize(Column& col_out, bool) override {
      Column col_tmp;
      RiGb res;
      RowIndex ri;
      Groupby gb;
      if (!is_const_) {
        res = group({col_}, {SortFlag::NONE});
        ri = std::move(res.first);
        gb = std::move(res.second);
      }

      // If there is one group only, fill it with constants or NA's.
      if (is_const_ || gb.size() == 1) {
        if (col_.get_element_isvalid(0)) {
          col_tmp = Column(new ConstInt_ColumnImpl(
                      col_.nrows(),
                      (nquantiles_ - 1) / 2,
                      SType::INT32
                    ));
        } else {
          col_tmp = Column(new ConstNa_ColumnImpl(
                      col_.nrows(), SType::INT32
                    ));
        }
        col_tmp.materialize(); // TODO: avoid materialization.
        col_out = std::move(col_tmp);
        return;
      }

      col_tmp = Column::new_data_column(col_.nrows(), SType::INT32);
      auto data_tmp = static_cast<int32_t*>(col_tmp.get_data_editable());

      // Check if there is an NA group.
      bool has_na_group;
      {
        size_t row;
        bool row_valid = ri.get_element(0, &row);
        xassert(row_valid); (void) row_valid;
        has_na_group = !col_.get_element_isvalid(row);
      }

      // Set up number of valid groups and the quantile coefficients.
      size_t ngroups = gb.size() - has_na_group;
      double a, b;
      constexpr double epsilon = static_cast<double>(
                                   std::numeric_limits<float>::epsilon()
                                 );
      if (ngroups == 1) {
        a = 0;
        b = (nquantiles_ - 1) / 2;
      } else {
        a =  nquantiles_ * (1 - epsilon) / static_cast<double>(ngroups - 1);
        b = -a * has_na_group;
      }

      dt::parallel_for_dynamic(gb.size(),
        [&](size_t i) {
          bool is_na_group = (i == 0 && has_na_group);
          size_t j0, j1;

          auto q = is_na_group? GETNA<int32_t>()
                              : static_cast<int32_t>(a * static_cast<double>(i) + b);

          gb.get_group(i, &j0, &j1);

          for (size_t j = j0; j < j1; ++j) {
            size_t row;
            bool row_valid = ri.get_element(j, &row);
            xassert(row_valid); (void) row_valid;
            data_tmp[row] = q;
          }
      });

      // Note, this assignment shoud be done at the very end,
      // as it destroys the current object, including `col_`,
      // `nquantiles_`, `ri_` and `gb_` members.
      col_out = std::move(col_tmp);
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
