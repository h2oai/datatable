//------------------------------------------------------------------------------
// Copyright 2019-2021 H2O.ai
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
#ifndef dt_COLUMN_COUNTNA_ALLROWS_h
#define dt_COLUMN_COUNTNA_ALLROWS_h
#include "column/virtual.h"
#include "stype.h"
namespace dt {


class CountAllRows_ColumnImpl : public Virtual_ColumnImpl {
  protected:
    Groupby gby_;

  public:
    CountAllRows_ColumnImpl(const Groupby& gby)
      : Virtual_ColumnImpl(gby.size(), SType::INT64),
        gby_(gby)
    {}


    ColumnImpl *clone() const override {
      return new CountAllRows_ColumnImpl(Groupby(gby_));
    }

    size_t n_children() const noexcept override {
      return 0;
    }


    bool get_element(size_t i, int64_t* out) const override {
      size_t i0, i1;
      this->gby_.get_group(i, &i0, &i1);
      *out = static_cast<int64_t>(i1 - i0);
      return true;
    }
};




}  // namespace dt
#endif
