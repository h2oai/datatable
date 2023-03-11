//------------------------------------------------------------------------------
// Copyright 2023 H2O.ai
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
#ifndef dt_COLUMN_COUNTALLROWS_h
#define dt_COLUMN_COUNTALLROWS_h
#include "column/virtual.h"
#include "parallel/api.h"
#include "stype.h"
namespace dt {

class CountAllRows_ColumnImpl : public Virtual_ColumnImpl {
  private:
    Groupby gby_;

  public:
    CountAllRows_ColumnImpl(const Groupby& gby)
      : Virtual_ColumnImpl(gby.size(), SType::INT64),
        gby_(gby)
    {}


    ColumnImpl* clone() const override {
      return new CountAllRows_ColumnImpl(gby_);
    }


    size_t n_children() const noexcept override {
      return 0;
    }

    void materialize(Column &col_out, bool) override {
      size_t nrows = gby_.size();
      const int32_t* offsets = gby_.offsets_r();
      Column col = Column::new_data_column(nrows, SType::INT64);
      auto data = static_cast<int64_t*>(col.get_data_editable());
      dt::parallel_for_dynamic(gby_.size(),
        [&](size_t gi) {
          for (size_t i = 0; i < nrows; ++i) {
            data[i] = offsets[i + 1] - offsets[i];
          }
        }
      );

      col_out = std::move(col);
    }

};


}  // namespace dt


#endif
