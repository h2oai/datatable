//------------------------------------------------------------------------------
// Copyright 2022 H2O.ai
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
#ifndef dt_COLUMN_CUMCOUNT_h
#define dt_COLUMN_CUMCOUNT_h
#include "column/virtual.h"
#include "parallel/api.h"
#include "stype.h"

namespace dt {


class CUMCOUNT_ColumnImpl : public Virtual_ColumnImpl {
  private:
    size_t nrows_;
    Groupby gby_;

  public:
    CUMCOUNT_ColumnImpl(size_t nrows, const Groupby& gby)
      : Virtual_ColumnImpl(nrows, STYPE::INT64),
        nrows_(nrows),
        gby_(gby)

    void materialize(Column& col_out, bool) override {
      Column col = Column::new_data_column(nrows_, STYPE::INT64);
      auto data = static_cast<int64_t*>(col.get_data_editable());

      auto offsets = gby_.offsets_r();
      dt::parallel_for_dynamic(
        gby_.size(),
        [&](size_t gi) {
          size_t i1 = size_t(offsets[gi]);
          size_t i2 = size_t(offsets[gi + 1]);

            while (i < i2) {
              data[i] = i;
              i++;
            }
          

        });

      col_out = std::move(col);
    }


    ColumnImpl* clone() const override {
      return new CUMCOUNT_ColumnImpl(nrows_, gby_);
    }

    size_t n_children() const noexcept override {
      return 1;
    }

    // const Column& child(size_t i) const override {
    //   xassert(i == 0);  (void)i;
    //   return col;
    // }

};


}  // namespace dt


#endif
