//------------------------------------------------------------------------------
// Copyright 2018-2020 H2O.ai
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
//
// See http://quick-bench.com/gl2wXMVIU4i4eswQL2dBg2oKCZs for variations
//
//------------------------------------------------------------------------------
#ifndef dt_SORT_INSERT_SORT_h
#define dt_SORT_INSERT_SORT_h
#include <algorithm>       // std::stable_sort
#include <functional>      // std::function
#include <type_traits>     // std::is_convertible
#include "sort/common.h"   // array
namespace dt {
namespace sort {



/**
  * insert_sort(ordering_in, ordering_out, compare)
  *
  * Sort vector `ordering_in` and store the sorted values into
  * `ordering_out` (both vectors must have the same size). It is also
  * allowed for `ordering_in` to be an empty vector, in which case
  * we treat it as if it was the sequence {0, 1, 2, ..., n-1}.
  *
  * The values in vector `ordering_in` are not compared directly,
  * instead we use the `compare` function with signature
  * `(size_t, size_t) -> bool`. This function compares the underlying
  * values at indices `i` and `j` and returns true if and only if
  * `value[i] < value[j]`. The indices `i, j` passed to this function
  * are in the range `[0; n)` (where `n` is the size of
  * `ordering_out`). Notably, these indices do not take `ordering_in`
  * into account.
  *
  */
template <typename TO, typename Compare>
void insert_sort(array<TO> ordering_in,
                 array<TO> ordering_out,
                 Compare compare)
{
  static_assert(
    std::is_convertible<Compare, std::function<bool(size_t, size_t)>>::value,
    "Invalid signature of comparator function");

  size_t n = ordering_out.size;
  xassert(n > 0);
  if (ordering_in.size) {
    xassert(ordering_in.size == n);
    xassert(ordering_in.ptr);
  }

  TO* const oo = ordering_out.ptr;
  oo[0] = 0;
  for (size_t i = 1; i < n; ++i) {
    size_t j = i;
    while (j && compare(i, static_cast<size_t>(oo[j - 1]))) {
      oo[j] = oo[j - 1];
      j--;
    }
    oo[j] = static_cast<TO>(i);
  }

  if (ordering_in.size) {
    for (size_t i = 0; i < n; ++i) {
      oo[i] = ordering_in.ptr[oo[i]];
    }
  }
}



/**
  * std_sort(ordering_in, ordering_out, compare)
  *
  * Same as `insert_sort()`, but uses stable_sort algorithm from the
  * standard library (typically this is a quick-sort implementation).
  */
template <typename TO, typename Compare>
void std_sort(array<TO> ordering_in,
              array<TO> ordering_out,
              Compare compare)
{
  size_t n = ordering_out.size;
  xassert(n > 0);
  if (ordering_in.size) {
    xassert(ordering_in.size == n);
    xassert(ordering_in.ptr);
  }

  TO* const oo = ordering_out.ptr;
  for (size_t i = 0; i < n; ++i) {
    oo[i] = static_cast<TO>(i);
  }
  std::stable_sort(oo, oo + n, compare);

  if (ordering_in.size) {
    for (size_t i = 0; i < n; ++i) {
      oo[i] = ordering_in.ptr[oo[i]];
    }
  }
}



/**
  * For small `n`s this function uses the insert-sort algorithm, while
  * for larger `n`s the std-sort implementation. In both cases this
  * function is single-threaded and thus should only be used for
  * relatively small `n`s.
  */
template <typename TO, typename Compare>
void small_sort(array<TO> ordering_in,
                array<TO> ordering_out,
                Compare compare)
{
  if (ordering_out.size < INSERTSORT_NROWS) {
    insert_sort(ordering_in, ordering_out, compare);
  }
  else {
    std_sort(ordering_in, ordering_out, compare);
  }
}




}}  // namespace dt::sort
#endif
