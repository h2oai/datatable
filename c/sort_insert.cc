//------------------------------------------------------------------------------
// Copyright 2018 H2O.ai
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
// Insertion sort functions
//
// Insert Sort algorithm has O(n²) complexity, and therefore should only be
// used for small arrays.
//
// See also:
//   - https://en.wikipedia.org/wiki/Insertion_sort
//   - datatable/microbench/insertsort
//------------------------------------------------------------------------------
#include <cstdlib>  // std::abs
#include <cstring>  // std::memcpy
#include "column.h"
#include "sort.h"



//==============================================================================
// Helper functions
//==============================================================================

/**
 * Compare two strings a and b (the validity flag for each string is passed
 * separately). The `strstart` variable instructs to perform comparisons
 * starting from that offset in the string. If `strstart` is larger than
 * the length of a string, that string is considered empty.
 *
 * Return 0 if strings are equal, R if a < b, or -R if a > b. An NA string
 * compares equal to another NA string, and less than any non-NA string. An
 * empty string compares greater than NA, but less than any non-empty string.
 */
template <int R>
int compare_strings(const CString& a, bool a_valid,
                    const CString& b, bool b_valid, size_t strstart)
{
  if (!(a_valid && b_valid)) return b_valid - a_valid;
  const size_t a_len = static_cast<size_t>(a.size);
  const size_t b_len = static_cast<size_t>(b.size);
  const bool a_isempty = (a_len <= strstart);
  const bool b_isempty = (b_len <= strstart);
  if (a_isempty || b_isempty) return (a_isempty - b_isempty) * R;

  for (size_t i = strstart; i < a_len; ++i) {
    if (i == b_len) return -R;
    char a_ch = a.ch[i];
    char b_ch = b.ch[i];
    if (a_ch == b_ch) continue;
    return (static_cast<uint8_t>(a_ch) < static_cast<uint8_t>(b_ch))? R : -R;
  }
  return a_len == b_len ? 0 : R;
}




//==============================================================================
// Insertion sort of arrays with primitive C types
//==============================================================================

/**
 * insert_sort_values<T, V>(x, o, n)
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
void insert_sort_values(const T* x, V* o, int n, GroupGatherer& gg)
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
  if (gg) {
    gg.from_data(x, o, static_cast<size_t>(n));
  }
}


/**
 * insert_sort_keys<T, V>(x, o, tmp, n)
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
void insert_sort_keys(const T* x, V* o, V* tmp, int n, GroupGatherer& gg)
{
  insert_sort_values(x, tmp, n, gg);
  for (int i = 0; i < n; ++i) {
    tmp[i] = o[tmp[i]];
  }
  std::memcpy(o, tmp, static_cast<size_t>(n) * sizeof(V));
}



//==============================================================================
// Insertion sort of string arrays
//==============================================================================

// For the string sorting procedure `insert_sort_?_str` the argument `x` is
// replaced with a triple `strdata`, `stroffs`, `strstart`. The first is a
// pointer to a memory buffer containing the string data. The `stroffs` is an
// array of offsets within `strdata` (each `stroffs[i]` gives the end of
// string `i`; the beginning of the first string is at offset `stroffs[-1]`).
// Finally, parameter `strstart` instructs to compare the strings starting from
// that byte.
//
template <typename V>
void insert_sort_keys_str(
    const Column& column, size_t strstart, V* o, V* tmp, int n,
    GroupGatherer& gg, bool descending)
{
  CString i_value, k_value;
  bool i_valid, k_valid;
  auto compfn = descending? compare_strings<-1> : compare_strings<1>;
  int j;
  tmp[0] = 0;
  for (int i = 1; i < n; ++i) {
    i_valid = column.get_element(static_cast<size_t>(o[i]), &i_value);
    for (j = i; j > 0; --j) {
      auto k = tmp[j - 1];
      k_valid = column.get_element(static_cast<size_t>(o[k]), &k_value);
      int cmp = compfn(i_value, i_valid, k_value, k_valid, strstart);
      if (cmp != 1) break;
      tmp[j] = tmp[j-1];
    }
    tmp[j] = static_cast<V>(i);
  }
  for (int i = 0; i < n; ++i) {
    tmp[i] = o[tmp[i]];
  }
  if (gg) {
    gg.from_data(column, tmp, static_cast<size_t>(n));
  }
  std::memcpy(o, tmp, static_cast<size_t>(n) * sizeof(V));
}


template <typename V>
void insert_sort_values_str(
    const Column& column, size_t strstart, V* o, int n,
    GroupGatherer& gg, bool descending)
{
  CString i_value, k_value;
  bool i_valid, k_valid;
  auto compfn = descending? compare_strings<-1> : compare_strings<1>;
  int j;
  o[0] = 0;
  for (int i = 1; i < n; ++i) {
    i_valid = column.get_element(static_cast<size_t>(i), &i_value);
    for (j = i; j > 0; j--) {
      auto k = o[j - 1];
      k_valid = column.get_element(static_cast<size_t>(k), &k_value);
      int cmp = compfn(i_value, i_valid, k_value, k_valid, strstart);
      if (cmp != 1) break;
      o[j] = o[j-1];
    }
    o[j] = static_cast<V>(i);
  }
  if (gg) {
    gg.from_data(column, o, static_cast<size_t>(n));
  }
}



//==============================================================================
// Explicitly instantiate template functions
//==============================================================================

template void insert_sort_keys(const uint8_t*,  int32_t*, int32_t*, int, GroupGatherer&);
template void insert_sort_keys(const uint16_t*, int32_t*, int32_t*, int, GroupGatherer&);
template void insert_sort_keys(const uint32_t*, int32_t*, int32_t*, int, GroupGatherer&);
template void insert_sort_keys(const uint64_t*, int32_t*, int32_t*, int, GroupGatherer&);

template void insert_sort_values(const uint8_t*,  int32_t*, int, GroupGatherer&);
template void insert_sort_values(const uint16_t*, int32_t*, int, GroupGatherer&);
template void insert_sort_values(const uint32_t*, int32_t*, int, GroupGatherer&);
template void insert_sort_values(const uint64_t*, int32_t*, int, GroupGatherer&);

template void insert_sort_keys_str(const Column&, size_t, int32_t*, int32_t*, int, GroupGatherer&, bool);
template void insert_sort_values_str(const Column&, size_t, int32_t*, int, GroupGatherer&, bool);

template int compare_strings<1>(const CString&, bool, const CString&, bool, size_t);
template int compare_strings<-1>(const CString&, bool, const CString&, bool, size_t);
