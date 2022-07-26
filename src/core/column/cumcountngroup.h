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
#ifndef dt_COLUMN_CUMCOUNTNGROUP_h
#define dt_COLUMN_CUMCOUNTNGROUP_h
#include "column/virtual.h"
#include "parallel/api.h"
#include "stype.h"
namespace dt {


template<bool CUMCOUNT, bool REVERSE>
class CumcountNgroup_ColumnImpl : public Virtual_ColumnImpl {
  private:
    Groupby gby_;

  public:
    CumcountNgroup_ColumnImpl(const Groupby& gby)
      : Virtual_ColumnImpl(gby.last_offset(), SType::INT64),
        gby_(gby)
    {}


    ColumnImpl* clone() const override {
      return new CumcountNgroup_ColumnImpl(gby_);
    }


    size_t n_children() const noexcept override {
      return 0;
    }


    void materialize(Column &col_out, bool) override {
      Column col = Column::new_data_column(nrows_, SType::INT64);
      auto data = static_cast<int64_t*>(col.get_data_editable());
      dt::parallel_for_dynamic(gby_.size(),
        [&](size_t gi) {
          size_t i1, i2;
          gby_.get_group(gi, &i1, &i2);
          for (size_t i = i1; i < i2; ++i) {
            if (REVERSE) {
              data[i] = CUMCOUNT? static_cast<int64_t>(i2 - i - 1)
                                : static_cast<int64_t>(gby_.size() - gi - 1);
            } else {
              data[i] = CUMCOUNT? static_cast<int64_t>(i - i1)
                                : static_cast<int64_t>(gi);
            }
          }
        });

      col_out = std::move(col);
    }

};


}  // namespace dt


#endif
