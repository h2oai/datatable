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
#ifndef dt_SORT_GROUPER_h
#define dt_SORT_GROUPER_h
#include "sort/common.h"        // array<T>
#include "utils/function.h"     // dt::function
namespace dt {
namespace sort {


/**
 * Helper class to collect grouping information while sorting.
 *
 * The end product of this class is the array of cumulative group sizes. This
 * array will have 1 + ngroups elements, with the first element being 0, and
 * the last the total number of elements in the data being sorted/grouped.
 *
 * In order to accommodate parallel sorting, the array of group sizes is
 * provided externally, and is not managed by this class (only written to).
 *
 * Internal parameters
 * -------------------
 * data_
 *     The array of cumulative group sizes. The array must be pre-allocated
 *     and passed to this class via `init()`.
 *
 * n_
 *     The number of groups that were stored in the `data_` array.
 *
 * offset_
 *     The total size of all groups added so far. This is always equals to
 *     `data_[n_ - 1]`.
 *
 */
template <typename T>
class Grouper
{
  using vec = array<T>;
  using compare_fn = dt::function<bool(size_t, size_t)>;

  private:
    vec data_;
    size_t offset_;
    size_t n_;

  public:
    Grouper(vec data, size_t initial_offset)
      : data_(data),
        offset_(initial_offset),
        n_(0) {}


    size_t size() const noexcept {
      return n_;
    }


    void fill_from_data(vec ordering, compare_fn cmp) {
      const size_t nrows = ordering.size();
      size_t last_i = 0;
      for (size_t i = 1; i < nrows; ++i) {
        if (cmp(last_i, i)) {
          push(i - last_i);
          last_i = i;
        }
      }
      push(nrows - last_i);
    }


  private:
    void push(size_t group_size) {
      xassert(n_ < data_.size());
      offset_ += group_size;
      data_[n_++] = offset_;
    }

};



}}  // namespace dt::sort
#endif
