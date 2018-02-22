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
 * insert_sort_fw<T,I>(x, o, oo, n)
 *
 * Sorts array `o` according to the values in `x` (both arrays must have the
 * same length `n`) and then returns it. Optionally, array `oo` of length `n`
 * may be provided, in which case it will be used as "scratch space" to use
 * during the sorting. If `o` is NULL, then it is treated as if it was
 * initialized with `o[i] == i`. In this case memory for array `o` will be
 * allocated and returned to the caller. The caller will have the ownership of
 * array `o`.
 *
 * For example, if `x` is {5, 2, -1, 7, 2}, then this function will leave `x`
 * unmodified but reorder the elements of `o` into {o[2], o[1], o[4], o[0],
 * o[3]}.
 *
 * The template function is parametrized by
 *   T: the type of the elements in the "keys" array `x`. This type should be
 *      comparable using standard operator "<".
 *   I: type of elements in the "output" array `o`. Since output array is
 *      usually an ordering, this type is either `int32_t` or `int64_t`.
 */
template <typename T, typename I>
I* insert_sort_fw(const T* x, I* o, I* oo, int n)
{
  bool oo_owned = false;
  if (!oo) {
    oo = new I[n];
    oo_owned = true;
  }
  oo[0] = 0;
  for (int i = 1; i < n; ++i) {
    T xival = x[i];
    int j = i;
    while (j && xival < x[oo[j - 1]]) {
      oo[j] = oo[j - 1];
      j--;
    }
    oo[j] = static_cast<I>(i);
  }
  if (o) {
    for (int i = 0; i < n; ++i) {
      oo[i] = o[oo[i]];
    }
    std::memcpy(o, oo, static_cast<size_t>(n) * sizeof(I));
    if (oo_owned) {
      delete[] oo;
    }
  } else {
    if (oo_owned) return oo;
    o = new I[n];
    std::memcpy(o, oo, static_cast<size_t>(n) * sizeof(I));
  }
  return o;
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

template int32_t* insert_sort_fw(const int8_t*,   int32_t*, int32_t*, int);
template int32_t* insert_sort_fw(const int16_t*,  int32_t*, int32_t*, int);
template int32_t* insert_sort_fw(const int32_t*,  int32_t*, int32_t*, int);
template int32_t* insert_sort_fw(const int64_t*,  int32_t*, int32_t*, int);
template int32_t* insert_sort_fw(const uint8_t*,  int32_t*, int32_t*, int);
template int32_t* insert_sort_fw(const uint16_t*, int32_t*, int32_t*, int);
template int32_t* insert_sort_fw(const uint32_t*, int32_t*, int32_t*, int);
template int32_t* insert_sort_fw(const uint64_t*, int32_t*, int32_t*, int);

