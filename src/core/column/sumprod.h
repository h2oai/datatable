//------------------------------------------------------------------------------
// Copyright 2022-2023 H2O.ai
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
#ifndef dt_COLUMN_SUMPROD_h
#define dt_COLUMN_SUMPROD_h
#include "column/reduce_unary.h"
#include "models/utils.h"   // ipow
namespace dt {


template <typename T, bool SUM, bool IS_GROUPED>
class SumProd_ColumnImpl : public ReduceUnary_ColumnImpl<T, T> {
  public:
    using ReduceUnary_ColumnImpl<T, T>::ReduceUnary_ColumnImpl;

    bool get_element(size_t i, T* out) const override {
      T result = !SUM; // 0 for `sum()` and 1 for `prod()`
      T value;
      size_t i0, i1;
      this->gby_.get_group(i, &i0, &i1);

      if (IS_GROUPED){
        size_t nrows = i1 - i0;
        bool is_valid = this->col_.get_element(i, &value);
        if (is_valid){
          result = SUM? static_cast<T>(nrows) * value
                      : ipow(value, nrows);
        }
      } else {
        for (size_t gi = i0; gi < i1; ++gi) {
          bool is_valid = this->col_.get_element(gi, &value);
          if (is_valid){
            result = SUM? result + value
                        : result * value;
          }
        }
      }

      *out = result;
      return true; // the result is never a missing value
    }
};


}  // namespace dt
#endif
