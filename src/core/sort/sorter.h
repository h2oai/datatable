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


/**
  * Virtual class that handles sorting of a column. Template parameter
  * <TO> corresponds to the type of indices in the resulting rowindex.
  */
template <typename TO>
class Sorter {
  protected:
    using ovec = array<TO>;
    size_t nrows_;

  public:
    Sorter(size_t n) {
      xassert(sizeof(TO) == 8 || n <= MAX_NROWS_INT32);
      nrows_ = n;
    }
    virtual ~Sorter() {}


    RowIndex sort() {
      Buffer rowindex_buf = Buffer::mem(nrows_ * sizeof(TO));
      ovec ordering_out(rowindex_buf);
      if (nrows_ <= INSERTSORT_NROWS) {
        small_sort(ordering_out);
      } else {
        radix_sort(ovec(), ordering_out, 0, true);
      }
      return RowIndex(std::move(rowindex_buf),
                      sizeof(TO) == 4? RowIndex::ARR32 : RowIndex::ARR64);
    }


  //----------------------------------------------------------------------------
  // API that should be implemented by the derived classes
  //----------------------------------------------------------------------------
  protected:
    /**
      * Sort the vector of indices `ordering_in` and write the result
      * into `ordering_out`. This method should be single-threaded,
      * and optimized for small `n`s.
      *
      * The sorting is performed according to the values of the
      * underlying column within the range `[offset; offset + n)`.
      *
      * The recommended way of implementing this methid is via the
      * `dt::sort::small_sort()` function from "sort/insert-sort.h".
      */
    virtual void small_sort(ovec ordering_in, ovec ordering_out,
                            size_t offset) const = 0;

    /**
      * Same as previous, but `ordering_in` is implicitly equal to
      * `{0, 1, ..., n-1}` and `offset` is 0.
      */
    virtual void small_sort(ovec ordering_out) const = 0;

    /**
      */
    virtual void radix_sort(ovec ordering_in, ovec ordering_out,
                            size_t offset, bool parallel) const = 0;

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
