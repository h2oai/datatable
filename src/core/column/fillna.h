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
#ifndef dt_COLUMN_FILLNA_h
#define dt_COLUMN_FILLNA_h
#include "column/virtual.h"
#include "parallel/api.h"
#include "stype.h"


namespace dt {

  //template <typename T>
  class FillNA_ColumnImpl : public Virtual_ColumnImpl {
  private:
    Column col_;
    bool direction_;
    Groupby gby_;

  public:
    FillNA_ColumnImpl(Column &&col, bool direction, const Groupby &gby)
      :Virtual_ColumnImpl(col.nrows(), col.stype()),
       col_(std::move(col)),
       direction_(direction),
       gby_(gby)
    {
      xassert(col_.can_be_read_as<T>());
    }

    void materialize(Column &col_out, bool) override {
      Column col = Column::new_data_column(col_.nrows(), col_.stype());
      auto data = static_cast<T*>(col.get_data_editable());

      auto offsets = gby_.offsets_r();
      dt::parallel_for_dynamic(
          gby_.size(),
          [&](size_t gi) {
            size_t i1 = size_t(offsets[gi]);
            size_t i2 = size_t(offsets[gi + 1]);

            T current; 
            T value = GETNA<T>();
            bool is_valid;
            if (direction_) {
                while (i1 < i2) {
                    is_valid = col_.get_element(i1, &current);
                    value = is_valid? current : value;
                    data[i1] = value;
                    i1++;
                }
            } 
            else {
                while (i2 > i1) {
                  is_valid = col_.get_element(i2-1, &current);
                  value = is_valid? current : value;
                  data[i2-1] = value;
                  i2--;
                }
            }
        });

      col_out = std::move(col);
    }

    ColumnImpl *clone() const override {
      return new FillNA_ColumnImpl(Column(col_), direction_, gby_);
    }

    size_t n_children() const noexcept override {
      return 1;
    }

    const Column &child(size_t i) const override {
      xassert(i == 0);
      (void)i;
      return col_;
    }
  };

}  // namespace dt


#endif
