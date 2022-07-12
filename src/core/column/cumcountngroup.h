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

template<bool CUMCOUNT>
class CUMCOUNTNGROUP_ColumnImpl : public Virtual_ColumnImpl {
  private:
    size_t n_rows_;
    bool reverse_;
    Groupby gby_;

  public:
    CUMCOUNTNGROUP_ColumnImpl(size_t n_rows, bool reverse, const Groupby& gby)
      : Virtual_ColumnImpl(n_rows, SType::INT64),
        n_rows_(n_rows),
        reverse_(reverse),
        gby_(gby)
      {}

    ColumnImpl* clone() const override {
      return new CUMCOUNTNGROUP_ColumnImpl(n_rows_, reverse_, gby_);
    }

    size_t n_children() const noexcept override {
      return 0;
    }

    const Column& child(size_t i) const override {
      xassert(i == 0);  (void)i;
    }

    bool get_element(size_t i, int64_t* out) const override{           
      if (gby_.size() == 1){
        *out = reverse_?n_rows_-1-i:i;
      } else {
          size_t low = 0;
          size_t high = gby_.size();
          auto offsets = gby_.offsets_r();
          while (low < high) {
            size_t mid = (low + high) / 2 ;
            if (i < offsets[mid]) {
              high = mid;
            } else {
              low = mid + 1;
            }
          }
          if (CUMCOUNT) {
            size_t i0, i1;
            gby_.get_group(low - 1, &i0, &i1);
            *out = reverse_?i1-1-i:i-i0;        
          } else {
            *out = reverse_?gby_.size()-low:low-1;        
          }
      }

      return true;

    }

};


}  // namespace dt


#endif
