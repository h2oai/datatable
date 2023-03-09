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
#ifndef dt_COLUMN_MINMAX_h
#define dt_COLUMN_MINMAX_h
#include "column/reduce_unary.h"
namespace dt {


template <typename T, bool MIN>
class MinMax_ColumnImpl : public ReduceUnary_ColumnImpl<T> {
  public:
    using ReduceUnary_ColumnImpl<T>::ReduceUnary_ColumnImpl;

    bool get_element(size_t i, T* out) const override {      
      // res` will be updated on the first valid element, due to `res_isna`
      // initially being set to `true`. So the default value here
      // only silences the compiler warning and makes the update
      // to happen a little bit faster, but it has no effect on the final result.
      T res = MIN ? std::numeric_limits<T>::max()
                  : std::numeric_limits<T>::min();
      bool res_isna = true;
      size_t i0, i1;
      this->gby_.get_group(i, &i0, &i1);

      for (size_t gi = i0; gi < i1; ++gi) {
        T value;
        bool isvalid = this->col_.get_element(gi, &value);
        if (MIN) {
          if (isvalid && (value < res || res_isna)) {
            res = value;
            res_isna = false;
          }
        } else {
          if (isvalid && (value > res || res_isna)) {
            res = value;
            res_isna = false;
          }
        }
      }

      *out = static_cast<U>(res);
      return !res_isna;
    }
};


}  // namespace dt

#endif
