//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
// Insertion sort functions
//
// Insert Sort algorithm has O(n²) complexity, and therefore should only be
// used for small arrays.
//
// See also:
//   - https://en.wikipedia.org/wiki/Insertion_sort
//   - datatable/microbench/insertsort
//------------------------------------------------------------------------------
#include "sort.h"
#include <cstring>  // std::memcpy



//==============================================================================
// Insertion sort of arrays with primitive C types
//==============================================================================

/**
 * insert_sort_keys_fw<T, V>(x, o, tmp, n)
 *
 * Sorts array `o` according to the values in `x` (both arrays must have the
 * same length `n`). Additionally, array `tmp` of length `n` must be provided,
 * which will be used as "scratch space" during sorting.
 *
 * For example, if `x` is {5, 2, -1, 7, 2}, then this function will leave `x`
 * unmodified but reorder the elements of `o` into {o[2], o[1], o[4], o[0],
 * o[3]}.
 *
 * The template function is parametrized by
 *   T: the type of the elements in the "keys" array `x`. This type should be
 *      comparable using standard operator "<".
 *   V: type of elements in the "output" array `o`. Since output array is
 *      usually an ordering, this type is either `int32_t` or `int64_t`.
 */

template <typename T, typename V>
void insert_sort_keys_fw(const T* x, V* o, V* tmp, int n)
{
  tmp[0] = 0;
  for (int i = 1; i < n; ++i) {
    T xival = x[i];
    int j = i;
    while (j && xival < x[tmp[j - 1]]) {
      tmp[j] = tmp[j - 1];
      j--;
    }
    tmp[j] = static_cast<V>(i);
  }
  for (int i = 0; i < n; ++i) {
    tmp[i] = o[tmp[i]];
  }
  std::memcpy(o, tmp, static_cast<size_t>(n) * sizeof(V));
}


/**
 * insert_sort_values_fw<T, V>(x, o, n)
 *
 * Sorts values in `x` and writes the ordering into `o`. Array `o` must be
 * pre-allocated and have the same length `n` as `x`.
 *
 * For example, if `x` is {5, 2, -1, 7, 2}, then this function will leave `x`
 * unmodified and write {2, 1, 4, 0, 3} into `o`.
 *
 * The template function is parametrized by
 *   T: the type of the elements in the "keys" array `x`. This type should be
 *      comparable using standard operator "<".
 *   V: type of elements in the "output" array `o`. Since output array is
 *      usually an ordering, this type is either `int32_t` or `int64_t`.
 */
template <typename T, typename V>
void insert_sort_values_fw(const T* x, V* o, int n)
{
  o[0] = 0;
  for (int i = 1; i < n; ++i) {
    T xival = x[i];
    int j = i;
    while (j && xival < x[o[j - 1]]) {
      o[j] = o[j - 1];
      j--;
    }
    o[j] = static_cast<V>(i);
  }
}



//==============================================================================
// Insertion sort of string arrays
//==============================================================================

// For the string sorting procedure `insert_sort_s4` the argument `x` is
// replaced with a triple `strdata`, `offs`, `skip`. The first is a pointer to
// a memory buffer containing the string data. The `offs` is an array of offsets
// within `strdata` (each `offs[i]` gives the end of string `i`; the beginning
// of the first string is at offset `offs[-1]`). Finally, parameter `skip`
// instructs to compare the strings starting from that byte.
//


//------------------------------------------------------------------------------
// Explicit instantation of template functions
//------------------------------------------------------------------------------

template void insert_sort_keys_fw(const uint8_t*,  int32_t*, int32_t*, int);
template void insert_sort_keys_fw(const uint16_t*, int32_t*, int32_t*, int);
template void insert_sort_keys_fw(const uint32_t*, int32_t*, int32_t*, int);
template void insert_sort_keys_fw(const uint64_t*, int32_t*, int32_t*, int);

template void insert_sort_values_fw(const int8_t*,   int32_t*, int);
template void insert_sort_values_fw(const int16_t*,  int32_t*, int);
template void insert_sort_values_fw(const int32_t*,  int32_t*, int);
template void insert_sort_values_fw(const int64_t*,  int32_t*, int);
template void insert_sort_values_fw(const uint8_t*,  int32_t*, int);
template void insert_sort_values_fw(const uint16_t*, int32_t*, int);
template void insert_sort_values_fw(const uint32_t*, int32_t*, int);
template void insert_sort_values_fw(const uint64_t*, int32_t*, int);
