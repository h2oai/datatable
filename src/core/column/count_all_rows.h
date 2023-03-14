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
#include "column/reduce_unary.h"
namespace dt {


class CountAllRows_ColumnImpl : public ReduceUnary_ColumnImpl<int64_t, int64_t, false> {
  public:
    using ReduceUnary_ColumnImpl<int64_t, int64_t, false>::ReduceUnary_ColumnImpl;

    bool get_element(size_t i, int64_t* out) const override {
      size_t i0, i1;
      this->gby_.get_group(i, &i0, &i1);
      *out = static_cast<int64_t>(i1 - i0);
      return true;
    }
};

}  // namespace dt
#endif
