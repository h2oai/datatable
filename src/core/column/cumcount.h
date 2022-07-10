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
    bool reverse_;
    Groupby gby_;

  public:
    CUMCOUNT_ColumnImpl(size_t nrows, bool reverse, const Groupby& gby)
      : Virtual_ColumnImpl(nrows, SType::INT64),
        nrows_(nrows),
        reverse_(reverse),
        gby_(gby)
      {}

    ColumnImpl* clone() const override {
      return new CUMCOUNT_ColumnImpl(nrows_, reverse_, gby_);
    }

    size_t n_children() const noexcept override {
      return 0;
    }

    const Column& child(size_t i) const override {
      xassert(i == 0);  (void)i;
    }

    bool get_element(size_t i, int64_t* out) const override{
      size_t i0, i1;
      for (size_t j = 0; j < gby_.size(); ++j){
         gby_.get_group(j, &i0, &i1);
        if (i>=i0 & i<i1) {
          *out = reverse_?i1-1-i:i-i0;
          break;
         }
         else {
          continue;
         }

      }
      return true;

    }

};


}  // namespace dt


#endif
