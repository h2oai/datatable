//------------------------------------------------------------------------------
// Copyright 2020 H2O.ai
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
#ifndef dt_SORT_SORTER_h
#define dt_SORT_SORTER_h
#include "sort/common.h"
namespace dt {
namespace sort {


class SorterInterface {
  public:
    virtual ~SorterInterface() {}
    virtual RowIndex sort() = 0;
};


/**
 * Virtual class that handles sorting of a column.
 */
template <typename TO>
class Sorter : public SorterInterface {
  protected:
    size_t nrows_;

  public:
    Sorter(size_t n) {
      xassert(sizeof(TO) == 8 || n <= MAX_NROWS_INT32);
      nrows_ = n;
    }


    RowIndex sort() override {
      return RowIndex();
    }

  protected:
    virtual void insert_sort(array<TO> ordering_out) const = 0;
    virtual void radix_sort(array<TO> ordering_out, bool parallel) const = 0;

    /**
      * Comparator function that compares the values of the underlying
      * column at indices `i` and `j`. This function should return:
      *   - a negative value if `val[i] < val[j]`;
      *   - zero if `val[i] == val[j]`;
      *   - a positive value if `val[i] > val[j]`.
      */
    virtual int compare_lge(size_t i, size_t j) const = 0;
};



}}  // namespace dt::sort
#endif
