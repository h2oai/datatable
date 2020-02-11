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
#ifndef dt_SORT_INSERTSORT_h
#define dt_SORT_INSERTSORT_h
#include <algorithm>       // std::algorithm
#include <functional>      // std::function
#include "sort/common.h"   // array
namespace dt {
namespace sort {


static size_t NROWS_INSERT_SORT = 16;


/**
  * simple_sort(ordering_out, cmp_lt)
  *
  * Sort values in an abstract vector and store the ordering into the
  * array `ordering_out`.
  *
  * The input vector is not given directly; instead this method takes
  * a comparator function `cmp_lt(i, j)` which compares the values at
  * indices `i` and `j` and returns true iff `value[i] < value[j]`.
  *
  * For example, if the input vector is {5, 2, -1, 7, 2}, then this
  * function will write {2, 1, 4, 0, 3} into `ordering_out`.
  *
  * For small `n`s this function uses the insert-sort algorithm, while
  * for larger `n`s it falls back to the algorithm from the standard
  * library. In both cases this function is single-threaded and thus
  * only suitable for small `n`s.
  */
template <typename Compare>
void simple_sort(array<int32_t> ordering_out, Compare cmp_lt) {
  static_assert(
    std::is_convertible<Compare, std::function<bool(size_t, size_t)>>::value,
    "Invalid signature of comparator function");

  size_t n = ordering_out.size;
  if (n < NROWS_INSERT_SORT) {
    insert_sort(ordering_out, cmp_lt);
  }
  else {
    for (size_t i = 0; i < n; ++i) {
      ordering_out.ptr[i] = static_cast<int32_t>(i);
    }
    std::stable_sort(ordering_out.ptr, ordering_out.ptr + n, cmp_lt);
  }
}


/**
  * simple_sort(ordering_in, ordering_out, cmp_lt)
  *
  * Sort values in vector `ordering_in` and store the aorted values
  * into the array `ordering_out`.
  *
  * The values in the `ordering_in` vector are not compares directly,
  * instead a function `cmp_lt` is used which compares the values at
  * indices `i` and `j` and returns true iff `value[i] < value[j]`.
  *
  * For example, if the input vector is {5, 2, -1, 7, 2}, then this
  * function will write {2, 1, 4, 0, 3} into `ordering_out`.
  *
  * For small `n`s this function uses the insert-sort algorithm, while
  * for larger `n`s it falls back to the algorithm from the standard
  * library. In both cases this function is single-threaded and thus
  * only suitable for small `n`s.
  */
template <typename TO, typename Compare>
void simple_sort(array<TO> ordering_in, array<TO> ordering_out, Compare cmp_lt)
{
  xassert(ordering_in.size == ordering_out.size);
  simple_sort(ordering_out, cmp_lt);
  size_t n = ordering_out.size;
  for (size_t i = 0; i < n; ++i) {
    ordering_out.ptr[i] = ordering_in.ptr[ordering_out.ptr[i]];
  }
}



template <typename TO, typename Compare>
void insert_sort(array<TO> ordering_out, Compare cmp_lt) {
  TO* oo = ordering_out.ptr;
  size_t n = ordering_out.size;
  oo[0] = 0;
  for (size_t i = 1; i < n; ++i) {
    size_t j = i;
    while (j && cmp_lt(i, static_cast<size_t>(oo[j - 1]))) {
      oo[j] = oo[j - 1];
      j--;
    }
    oo[j] = static_cast<TO>(i);
  }
}



template <typename TO, typename Compare>
void insert_sort(array<TO> ordering_in, array<TO> ordering_out, Compare cmp_lt)
{
  insert_sort(ordering_out, cmp_lt);
  size_t n = ordering_out.size;
  for (size_t i = 0; i < n; ++i) {
    ordering_out.ptr[i] = ordering_in.ptr[ordering_out.ptr[i]];
  }
}




}}  // namespace dt::sort
#endif
