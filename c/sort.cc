//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
// Sorting/ordering functions.
//
// We use stable parallel MSD Radix sort algorithm, with fallback to Insertion
// sort at small `n`s.
//
// All functions defined here treat the input arrays as immutable -- i.e. no
// in-place sorting. Instead, each function creates and returns an "ordering"
// vector. The ordering `o` of an array `x` is such a sequence of integers that
// array
//     [x[o[i]] for i in range(n)]
// is sorted in ascending order. The sortings are stable, and will gather all NA
// values in `x` (if any) at the beginning of the sorted list.
//
// See also:
//      https://en.wikipedia.org/wiki/Radix_sort
//      https://en.wikipedia.org/wiki/Insertion_sort
//      http://stereopsis.com/radix.html
//      /microbench/sort
//
// Based on Radix sort implementation in (R)data.table:
//      https://github.com/Rdatatable/data.table/src/forder.c
//      https://github.com/Rdatatable/data.table/src/fsort.c
//
//
// Algorithm outline
// =================
//
// 1. Data preparation step.
//
//    Before data can be sorted, it has to be "prepared". The purpose of this
//    transformation is 1) to transform all types into unsigned integers which
//    can be sorted via radix-sort algorithm; 2) to collect values into a
//    contiguous buffer if they are originally shuffled by a RowIndex; 3) to
//    reduce the range of possible values and thus the amount of sorting that
//    has to be done; 4) to apply parameters `na_position` and `ascending`.
//
//    The result of this step is an array `x` of type `uintN_t[n]`, where N
//    depends on the input column. Thus, during this step we try to map the
//    original data into the unsigned integers, making sure that such
//    transformation preserves the order of the original elements.
//
//    For integers, this transformation is merely subtracting the min value and
//    relocating NA to the beginning of the range. For floats, we perform a
//    more complicated bit twiddling (see description below). For strings the
//    data cannot be prepared "fully", so we merely map the first byte into `x`.
//
// 2. Build histogram.
//
//    For each value in `x` take its `radix` most significant bits, and compute
//    the frequency table for these prefixes. The histogram is computed
//    separately for each chunk of the input used during parallel processing.
//    This step produces a cumulative `histogram` of the data, where for each
//    possible radix prefix and for each input chunk, the value in the histogram
//    is the location within the final sorted array where all values in `x` with
//    specific radix/chunk should go.
//
// 3. Reorder data.
//
//    Shuffle the input data according to the ordering implied by the histogram
//    computed in the previous step. Also, update values of `x` if further
//    sorting is needed. Here "updating" involves removing the initial radix
//    prefix (which was already sorted), sometimes also reducing the elemsize
//    of `x`. For strings we copy the next character of each string into the
//    buffer `x`.
//
// 4. Recursion.
//
//    At this point the data is already stably-sorted according to its most
//    significant `radix` bits (or for strings, by their first characters), and
//    the values in `x` are properly transformed to for further sorting. Also,
//    the `histogram` matrix from step 2 carries information about where each
//    pre-sorted group is located. All we need to do is to sort values within
//    each of those groups, and the job will be done.
//
//    Each sub-group that needs to be sorted will have a different size, and we
//    process each such group taking into account its size: for small groups
//    use insertion-sort and process all those groups in-parallel; larger groups
//    can be sorted in-parallel too, using radix-sort; largest groups are sorted
//    one-by-one, each one one with parallel radix-sort algorithm.
//
//
// Grouping
// ========
//
// Sorting naturally produces information about groups with same values, and
// this information is even necessary in order to sort by multiple columns.
//
// For small arrays that are sorted with insert-sort, group sizes can be
// computed simply by comparing adjacent values.
//
// For radix-sort, if there is no recursion step then group information can be
// derived from the histogram. If radix-sort recurses, then large subgroups
// (which are processed one-by-one) can simply push group sizes into the common
// group stack. However when smaller subgroups are processed in parallel, then
// each such processor needs to write its grouping information into a separate
// buffer, which are then merged into the main group stack.
//
//
//------------------------------------------------------------------------------
#include "sort.h"
#include <algorithm>  // std::min
#include <cstdlib>    // std::abs
#include <cstring>    // std::memset, std::memcpy
#include <stdio.h>    // printf
#include "column.h"
#include "datatable.h"
#include "rowindex.h"
#include "types.h"
#include "utils.h"
#include "utils/array.h"
#include "utils/assert.h"
#include "utils/omp.h"



/**
 * Data structure that holds all the variables needed to perform radix sort.
 * This object is passed around between the functions that represent different
 * steps in the sorting process, each function reading/writing some of these
 * parameters. The documentation for each function will describe which specific
 * parameters it writes.
 *
 * x
 *   The main data array, depending on `elemsize` has one of the following
 *   types: `uint8_t*`, `uint16_t*`, `uint32_t*` or `uint64_t*`. The array has
 *   `n` elements. This array serves as a "sorting key" -- the final goal is
 *   to produce the ordering of values in `x`.
 *   The elements in `x` are always unsigned, and will be sorted accordingly.
 *   In particular, the data must usually be transformed in order to ensure
 *   that it sorts correctly (this is done in `prepare_input` step).
 *
 * o
 *   Current ordering (row indices) of elements in `x`. This is an array of size
 *   `n` (same as `x`). If present, then this array will be sorted according
 *   to the values `x`. If nullptr, then it will be treated as if `o[j] == j`.
 *
 * n
 *   Number of elements in arrays `x` and `o`.
 *
 * elemsize
 *   Size in bytes of each element in `x`, one of: 1, 2, 4, or 8.
 *
 * strdata, stroffs
 *   For string columns only, these are pointers `col->strdata()` and
 *   `col->offsets()` respectively.
 *
 * strstart
 *   For string columns only, this is the position within the string that is
 *   currently being tested. More specifically, at the beginning of a
 *   radix_sort() call, `strstart` will indicate the offset within each string
 *   starting from each we need to sort. The assertion being that all prefixes
 *   before position `strstart` are already properly sorted.
 *
 * nsigbits
 *   Number of significant bits in the elements of `x`. This cannot exceed
 *   `8 * elemsize`, but could be less. This value is an assertion that all
 *   elements in `x` are in the range `[0; 2**nsigbits)`. The number of
 *   significant bits cannot be 0.
 *
 * shift
 *   The parameter of linear transform to be applied to each item in `x` to
 *   obtain the radix. That is, radix for element `i` is
 *       (x[i] >> shift)
 *   The `shift` can also be zero, indicating that the values themselves
 *   are the radixes (as in counting sort).
 *
 * nradixes
 *   Total number of possible radixes, equal to `1 << (nsigbits - shift)`.
 *
 * nth
 *   The number of threads used by OMP.
 *
 * nchunks, chunklen
 *   These variables describe how the total number of rows, `n`, will be split
 *   into smaller chunks for the parallel computation. In particular, the range
 *   `[0; n)` is divided into `nchunks` sub-ranges each except the last one
 *   having length `chunklen`. The last chunk will have the length
 *   `n - chunklen*(nchunks - 1)`. The number of chunks is >= 1, and `chunklen`
 *   is also positive.
 *
 * histogram
 *   Computed during the `build_histogram` step, this array will contain the
 *   histogram of data values, by chunk and by radix. More specifically, this
 *   is a `size_t*` array which is viewed as a 2D table. The `(i,k)`-th element
 *   of this array (where `i` is the chunk index, and `k` is radix) is located
 *   at index `(i * nradixes + k)` and represents the starting position within
 *   the output array where the elements with radix `k` and within the chunk `i`
 *   should be written. That is,
 *       histogram[i,k] = #{elements in chunks 0..i-1 with radix = k} +
 *                        #{elements in all chunks with radix < k}
 *   After the "reorder_data" step, this histogram is modified to contain values
 *       histogram[i,k] = #{elements in chunks 0..i with radix = k} +
 *                        #{elements in all chunks with radix < k}
 *
 * next_x
 *   When `shift > 0`, a single pass of the `radix_psort()` function will sort
 *   the data array only partially, and 1 or more extra passes will be needed to
 *   finish sorting. In this case the array `next_x` (of length `n`) will hold
 *   pre-sorted and potentially modified values of the original data array `x`.
 *   The array `next_x` is filled in the "reorder_data" step. If it was nullptr,
 *   the array will be allocated, otherwise its contents will be overwritten.
 *
 * next_o
 *   The reordered array `o` to be carried over to the next step, or to be
 *   returned as the final ordering in the end.
 *
 * next_elemsize
 *   Size in bytes of each element in `next_x`. This cannot be greater than
 *   `elemsize`, however `next_elemsize` can be 0.
 */
class SortContext {
  public:
    void*    x;
    int32_t* o;
    void*    next_x;
    int32_t* next_o;
    size_t*  histogram;
    const unsigned char *strdata;
    const int32_t *stroffs;
    size_t strstart;
    size_t n;
    size_t nth;
    size_t nchunks;
    size_t chunklen;
    size_t nradixes;
    int8_t elemsize;
    int8_t next_elemsize;
    int8_t nsigbits;
    int8_t shift;
    bool use_order;
    int : 24;

  SortContext()
    : x(nullptr), o(nullptr), next_x(nullptr), next_o(nullptr),
      histogram(nullptr), strdata(nullptr), stroffs(nullptr), strstart(0),
      n(0), nth(0), nchunks(0), chunklen(0), nradixes(0), elemsize(0),
      next_elemsize(0), nsigbits(0), shift(0), use_order(false) {}


  //============================================================================
  // Data preparation
  //============================================================================

  void initialize(const Column* col, arr32_t& order) {
    n = static_cast<size_t>(col->nrows);
    use_order = (bool) order;
    if (!use_order) order.resize(n);
    o = order.data();
    SType stype = col->stype();
    switch (stype) {
      case ST_BOOLEAN_I1: _initB(col); break;
      case ST_INTEGER_I1: _initI<int8_t,  uint8_t>(col); break;
      case ST_INTEGER_I2: _initI<int16_t, uint16_t>(col); break;
      case ST_INTEGER_I4: _initI<int32_t, uint32_t>(col); break;
      case ST_INTEGER_I8: _initI<int64_t, uint64_t>(col); break;
      case ST_REAL_F4:    _initF<uint32_t>(col); break;
      case ST_REAL_F8:    _initF<uint64_t>(col); break;
      case ST_STRING_I4_VCHAR: _initS<int32_t>(col); break;
      default:
        throw NotImplError() << "Unable to sort Column of stype " << stype;
    }
  }


  /**
   * Boolean columns have only 3 distinct values: -128, 0 and 1. The transform
   * `(x + 0xBF) >> 6` converts these to 0, 2 and 3 respectively, provided that
   * the addition is done as addition of unsigned bytes (i.e. modulo 256).
   */
  void _initB(const Column* col) {
    uint8_t* xi = static_cast<uint8_t*>(col->data());
    uint8_t* xo = new uint8_t[n];
    x = static_cast<void*>(xo);
    elemsize = 1;
    nsigbits = 2;

    if (use_order) {
      #pragma omp parallel for schedule(static)
      for (size_t j = 0; j < n; j++) {
        uint8_t t = xi[o[j]] + 0xBF;
        xo[j] = t >> 6;
      }
    } else {
      #pragma omp parallel for schedule(static)
      for (size_t j = 0; j < n; j++) {
        // xi[j]+0xBF should be computed as uint8_t; by default C++ upcasts it
        // to int, which leads to wrong results after shift by 6.
        uint8_t t = xi[j] + 0xBF;
        xo[j] = t >> 6;
      }
    }
  }


  /**
   * For integer columns we subtract the min value, thus making the column
   * unsigned. Depending on the range of the values (max - min + 1), we cast
   * the data into an appropriate smaller type.
   */
  template <typename T, typename TU>
  void _initI(const Column* col) {
    auto icol = static_cast<const IntColumn<T>*>(col);
    assert(sizeof(T) == sizeof(TU));
    T min = icol->min();
    T max = icol->max();
    nsigbits = sizeof(T) * 8;
    nsigbits -= dt::nlz(static_cast<TU>(max - min + 1));
    if (nsigbits > 32)      _initI_impl<T, TU, uint64_t>(icol, min);
    else if (nsigbits > 16) _initI_impl<T, TU, uint32_t>(icol, min);
    else if (nsigbits > 8)  _initI_impl<T, TU, uint16_t>(icol, min);
    else                    _initI_impl<T, TU, uint8_t >(icol, min);
  }

  template <typename T, typename TI, typename TO>
  void _initI_impl(const Column* col, T min) {
    TI una = static_cast<TI>(GETNA<T>());
    TI umin = static_cast<TI>(min);
    TI* xi = static_cast<TI*>(col->data());
    TO* xo = new TO[n];
    x = static_cast<void*>(xo);
    elemsize = sizeof(TO);
    next_elemsize = nsigbits > 48? 8 :
                    nsigbits > 32? 4 :
                    nsigbits > 16? 2 : 0;

    if (use_order) {
      #pragma omp parallel for schedule(static)
      for (size_t j = 0; j < n; ++j) {
        TI t = xi[o[j]];
        xo[j] = t == una? 0 : static_cast<TO>(t - umin + 1);
      }
    } else {
      #pragma omp parallel for schedule(static)
      for (size_t j = 0; j < n; j++) {
        TI t = xi[j];
        xo[j] = t == una? 0 : static_cast<TO>(t - umin + 1);
      }
    }
  }


  /**
   * For float32/64 we need to carefully manipulate the bits in order to present
   * them in the correct order as uint32/64. At bit level, the structure of
   * IEEE754-1985 float is the following (first 1 bit is the sign, next 8 bits
   * are the exponent, last 23 bits represent the significand):
   *      Bits (uint32 value)          Float value
   *      0 00 000000                  +0
   *      0 00 000001 - 0 00 7FFFFF    Denormal numbers (positive)
   *      0 01 000000 - 0 FE 7FFFFF    +1*2^-126 .. +1.7FFFFF*2^+126
   *      0 FF 000000                  +Inf
   *      0 FF 000001 - 0 FF 7FFFFF    NaNs (positive)
   *      1 00 000000                  -0
   *      1 00 000001 - 1 00 7FFFFF    Denormal numbers (negative)
   *      1 01 000000 - 1 FE 7FFFFF    -1*2^-126 .. -1.7FFFFF*2^+126
   *      1 FF 000000                  -Inf
   *      1 FF 000001 - 1 FF 7FFFFF    NaNs (negative)
   * In order to put these values into correct order, we'll do the following
   * transform:
   *      (1) numbers with sign bit = 0 will turn the sign bit on.
   *      (2) numbers with sign bit = 1 will be XORed with 0xFFFFFFFF
   *      (3) all NAs/NaNs will be converted to 0
   *
   * Float64 is similar: 1 bit for the sign, 11 bits of exponent, and finally
   * 52 bits of the significand.
   *
   * See also:
   *      https://en.wikipedia.org/wiki/Float32
   *      https://en.wikipedia.org/wiki/Float64
   */
  template <typename TO>
  void _initF(const Column* col) {
    TO* xi = static_cast<TO*>(col->data());
    TO* xo = new TO[n];
    x = static_cast<void*>(xo);
    elemsize = sizeof(TO);
    nsigbits = elemsize * 8;
    next_elemsize = sizeof(TO) == 8? 8 : 2;

    constexpr TO EXP = sizeof(TO) == 8? 0x7FF0000000000000ULL : 0x7F800000;
    constexpr TO SIG = sizeof(TO) == 8? 0x000FFFFFFFFFFFFFULL : 0x007FFFFF;
    constexpr TO SBT = sizeof(TO) == 8? 0x8000000000000000ULL : 0x80000000;
    constexpr int SHIFT = sizeof(TO) * 8 - 1;

    if (use_order) {
      #pragma omp parallel for schedule(static)
      for (size_t j = 0; j < n; j++) {
        TO t = xi[o[j]];
        xo[j] = ((t & EXP) == EXP && (t & SIG) != 0)
                ? 0 : t ^ (SBT | -(t>>SHIFT));
      }
    } else {
      #pragma omp parallel for schedule(static)
      for (size_t j = 0; j < n; j++) {
        TO t = xi[j];
        xo[j] = ((t & EXP) == EXP && (t & SIG) != 0)
                ? 0 : t ^ (SBT | -(t>>SHIFT));
      }
    }
  }


  /**
   * For strings, we fill array `x` with the values of the first 2 characters in
   * each string. We also set up auxiliary variables `strdata`, `stroffs`,
   * `strstart`.
   *
   * More specifically, for each string item, if it is NA then we map it to 0;
   * if it is an empty string we map it to 1, otherwise we map it to `ch[i] + 2`
   * where `ch[i]` is the i-th character of the string. This doesn't overflow
   * because in UTF-8 the largest legal byte is 0xF7.
   */
  template <typename T>
  void _initS(const Column* col) {
    auto scol = static_cast<const StringColumn<T>*>(col);
    strdata = reinterpret_cast<uint8_t*>(scol->strdata());
    T* offs = scol->offsets();
    stroffs = static_cast<int32_t*>(offs);  // ???
    strstart = 0;
    uint8_t* xo = new uint8_t[n];
    x = static_cast<void*>(xo);
    elemsize = 1;
    nsigbits = 8;

    int maxlen = 0;
    #pragma omp parallel for schedule(static) reduction(max:maxlen)
    for (size_t j = 0; j < n; ++j) {
      int32_t k = use_order? o[j] : static_cast<int32_t>(j);
      T offend = offs[k];
      if (offend < 0) {  // NA
        xo[j] = 0;
      } else {
        T offstart = std::abs(offs[k - 1]);
        T len = offend - offstart;
        xo[j] = len > 0? strdata[offstart] + 2 : 1;
        if (len > maxlen) maxlen = len;
      }
    }
    next_elemsize = (maxlen > 1);
  }



  //============================================================================
  // Histograms
  //============================================================================

  /**
   * Calculate initial histograms of values in `x`. Specifically, we're creating
   * the `histogram` table which has `nchunks` rows and `nradix` columns. Cell
   * `[i,j]` in this table will contain the count of values `x` within the chunk
   * `i` such that the topmost `nradixbits` of `x` are equal to `j`. After that
   * the values are cumulated across all `j`s (i.e. in the end the histogram
   * will contain cumulative counts of values in `x`).
   */
  void build_histogram() {
    size_t counts_size = nchunks * nradixes;
    if (!histogram) {
      histogram = new size_t[counts_size];
    }
    std::memset(histogram, 0, counts_size * sizeof(size_t));
    switch (elemsize) {
      case 1: _histogram_gather<uint8_t>();  break;
      case 2: _histogram_gather<uint16_t>(); break;
      case 4: _histogram_gather<uint32_t>(); break;
      case 8: _histogram_gather<uint64_t>(); break;
    }
    _histogram_cumulate();
  }

  template<typename T> void _histogram_gather() {
    T* tx = static_cast<T*>(x);
    #pragma omp parallel for schedule(dynamic) num_threads(nth)
    for (size_t i = 0; i < nchunks; ++i) {
      size_t* cnts = histogram + (nradixes * i);
      size_t j0 = i * chunklen;
      size_t j1 = std::min(j0 + chunklen, n);
      for (size_t j = j0; j < j1; ++j) {
        cnts[tx[j] >> shift]++;
      }
    }
  }

  void _histogram_cumulate() {
    size_t cumsum = 0;
    size_t counts_size = nchunks * nradixes;
    for (size_t j = 0; j < nradixes; ++j) {
      for (size_t r = j; r < counts_size; r += nradixes) {
        size_t t = histogram[r];
        histogram[r] = cumsum;
        cumsum += t;
      }
    }
  }



  //============================================================================
  // Reorder data
  //============================================================================

  /**
   * Perform the main radix shuffle, filling in arrays `next_o` and `next_x`.
   * The array `next_o` will contain the original row numbers of the values in
   * `x` such that `x[next_o]` is sorted with respect to the most significant
   * bits. The `next_x` array will contain the sorted elements of `x`, with MSB
   * bits already removed -- we need it mostly for page-efficiency at later
   * stages of the algorithm.
   *
   * During execution of this step we also modify the `histogram` array so that
   * by the end of the step each cell in `histogram` will contain the offset
   * past the *last* item within that cell. In particular, the last row of the
   * `histogram` table will contain end-offsets of output regions corresponding
   * to each radix value.
   */
  void reorder_data() {
    if (!next_x && next_elemsize) {
      size_t sz = static_cast<size_t>(next_elemsize);
      // Allocate as `int64_t` to ensure 8-byte alignment
      next_x = new int64_t[(n * sz + 7) / 8];
    }
    if (!next_o) {
      next_o = new int32_t[n];
    }
    if (strdata) {
      if (next_x) _reorder_str();
      else _reorder_impl<uint8_t, char, false>();
    } else {
      switch (elemsize) {
        case 8:
          if (next_elemsize == 8) _reorder_impl<uint64_t, uint64_t, true>();
          if (next_elemsize == 4) _reorder_impl<uint64_t, uint32_t, true>();
          break;
        case 4: _reorder_impl<uint32_t, uint16_t, true>(); break;
        case 2: _reorder_impl<uint16_t, char, false>(); break;
        case 1: _reorder_impl<uint8_t,  char, false>(); break;
      }
    }
    std::swap(x, next_x);
    std::swap(o, next_o);
    use_order = true;
  }

  template<typename TI, typename TO, bool OUT> void _reorder_impl() {
    TI mask = static_cast<TI>((1ULL << shift) - 1);
    TI* xi = static_cast<TI*>(x);
    TO* xo = static_cast<TO*>(next_x);
    #pragma omp parallel for schedule(dynamic) num_threads(nth)
    for (size_t i = 0; i < nchunks; ++i) {
      size_t j0 = i * chunklen;
      size_t j1 = std::min(j0 + chunklen, n);
      size_t* tcounts = histogram + (nradixes * i);
      for (size_t j = j0; j < j1; ++j) {
        size_t k = tcounts[xi[j] >> shift]++;
        next_o[k] = use_order? o[j] : static_cast<int32_t>(j);
        if (OUT) {
          xo[k] = static_cast<TO>(xi[j] & mask);
        }
      }
    }
    assert(histogram[nchunks * nradixes - 1] == n);
  }

  void _reorder_str() {
    uint8_t* xi = static_cast<uint8_t*>(x);
    uint8_t* xo = static_cast<uint8_t*>(next_x);
    const int32_t ss = static_cast<int32_t>(strstart) + 1;

    int32_t maxlen = 0;
    #pragma omp parallel for schedule(dynamic) num_threads(nth) \
            reduction(max:maxlen)
    for (size_t i = 0; i < nchunks; ++i) {
      size_t j0 = i * chunklen;
      size_t j1 = std::min(j0 + chunklen, n);
      size_t* tcounts = histogram + (nradixes * i);
      for (size_t j = j0; j < j1; ++j) {
        size_t k = tcounts[xi[j]]++;
        int32_t w = use_order? o[j] : static_cast<int32_t>(j);
        int32_t offend = stroffs[w];
        int32_t offstart = std::abs(stroffs[w - 1]) + ss;
        int32_t len = offend - offstart;
        xo[k] = len > 0? strdata[offstart] + 2 : 1;
        next_o[k] = w;
        if (len > maxlen) maxlen = len;
      }
    }
    next_elemsize = maxlen > 0;
    assert(histogram[nchunks * nradixes - 1] == n);
  }


};



//==============================================================================
// Forward declarations
//==============================================================================

static void insert_sort(SortContext*);
static void radix_psort(SortContext*);

#define INSERT_SORT_THRESHOLD 64

typedef struct radix_range { size_t size, offset; } radix_range;
static int _rrcmp(const void *a, const void *b) {
  const size_t x = *(const size_t*)a;
  const size_t y = *(const size_t*)b;
  return (x < y) - (y < x);
}



//==============================================================================
// Main sorting routine
//==============================================================================

/**
 * Sort the column, and return its ordering as a RowIndex object. This function
 * will choose the most appropriate algorithm for sorting. The data in column
 * `col` will not be modified.
 *
 * The function returns nullptr if there is a runtime error (for example an
 * intermediate buffer cannot be allocated).
 */
RowIndex DataTable::sortby(const arr32_t& colindices, bool /*make_groups*/) const
{
  if (colindices.size() != 1) {
    throw NotImplError() << "Sorting by multiple columns is not supported yet";
  }
  if (nrows > INT32_MAX) {
    throw NotImplError() << "Cannot sort a datatable with " << nrows << " rows";
  }
  if (rowindex.isarr64() || rowindex.length() > INT32_MAX ||
      rowindex.max() > INT32_MAX) {
    throw NotImplError() << "Cannot sort a datatable which is based on a "
                            "datatable with >2**31 rows";
  }
  if (nrows <= 1) {
    return RowIndex::from_slice(0, nrows, 1);
  }
  Column* col0 = columns[colindices[0]];
  int32_t irows = static_cast<int32_t>(nrows);
  size_t  zrows = static_cast<size_t>(nrows);
  arr32_t order = rowindex.extract_as_array32();

  if (nrows <= INSERT_SORT_THRESHOLD) {
    SType stype = col0->stype();
    if (stype == ST_REAL_F4 || stype == ST_REAL_F8 || !rowindex.isabsent()) {
      SortContext sc;
      sc.initialize(col0, order);
      insert_sort(&sc);
      dtfree(sc.x);
    } else if (stype == ST_STRING_I4_VCHAR) {
      auto scol = static_cast<const StringColumn<int32_t>*>(col0);
      const uint8_t* strdata = reinterpret_cast<const uint8_t*>(scol->strdata());
      const int32_t* offs = scol->offsets();
      order.resize(zrows);
      int32_t* o = order.data();
      insert_sort_values_str(strdata, offs, 0, o, irows);
      return RowIndex::from_array32(std::move(order));
    } else {
      order.resize(zrows);
      int32_t* o = order.data();
      void* x = col0->data();
      switch (stype) {
        case ST_BOOLEAN_I1: insert_sort_values_fw(static_cast<int8_t*>(x), o, irows); break;
        case ST_INTEGER_I1: insert_sort_values_fw(static_cast<int8_t*>(x), o, irows); break;
        case ST_INTEGER_I2: insert_sort_values_fw(static_cast<int16_t*>(x), o, irows); break;
        case ST_INTEGER_I4: insert_sort_values_fw(static_cast<int32_t*>(x), o, irows); break;
        case ST_INTEGER_I8: insert_sort_values_fw(static_cast<int64_t*>(x), o, irows); break;
        default: throw ValueError() << "Insert sort not implemented for column of stype " << stype;
      }
      return RowIndex::from_array32(std::move(order));
    }
  } else {
    SortContext sc;
    sc.initialize(col0, order);
    radix_psort(&sc);
    free(sc.x);
    free(sc.next_x);
    delete[] sc.next_o;
    delete[] sc.histogram;
  }
  return RowIndex::from_array32(std::move(order));
}



//==============================================================================
// Radix sort functions
//==============================================================================

/**
 * Determine how the input should be split into chunks: at least as many
 * chunks as the number of threads, unless the input array is too small
 * and chunks become too small in size. We want to have more than 1 chunk
 * per thread so as to reduce delays caused by uneven execution time among
 * threads, on the other hand too many chunks should be avoided because that
 * would increase the time needed to combine the results from different
 * threads.
 * Also compute the desired radix size, as a function of `nsigbits` (number of
 * significant bits in the data column).
 *
 * SortContext inputs:
 *      n, nsigbits
 *
 * SortContext outputs:
 *      nth, nchunks, chunklen, shift, nradixes
 */
static void determine_sorting_parameters(SortContext *sc)
{
  size_t nth = (size_t) omp_get_num_threads();
  size_t nch = nth * 2;
  size_t maxchunklen = 1024;
  sc->nth = nth;
  sc->chunklen = std::max((sc->n + nch - 1) / nch, maxchunklen);
  sc->nchunks = (sc->n - 1)/sc->chunklen + 1;

  int8_t nradixbits = sc->nsigbits < 16 ? sc->nsigbits : 16;
  if (!sc->shift) sc->shift = sc->nsigbits - nradixbits;
  sc->nradixes = 1 << nradixbits;
}



/**
 * Sort array `o` of length `n` by values `x`, using MSB Radix sort algorithm.
 * Array `o` will be modified in-place. If `o` is nullptr, then it will be
 * allocated and filled with values of `range(n)`.
 *
 * SortContext inputs:
 *      x:      buffer of size `elemsize * n`
 *      next_x: nullptr, or buffer of size `next_elemsize * n`
 *      o:      nullptr, or array of `int32_t`s of length `n`
 *      next_o: nullptr, or array of `int32_t`s of length `n`
 *      elemsize
 *      next_elemsize: (may be zero)
 *      nsigbits
 *
 * SortContext outputs:
 *      o:  If nullptr, this array will be allocated; otherwise its contents will
 *          be modified in-place.
 *      x, next_x, next_o, histogram: These arrays may be allocated, or their
 *          contents may be altered arbitrarily.
 */
static void radix_psort(SortContext *sc)
{
  int32_t* ores = sc->o;
  determine_sorting_parameters(sc);
  sc->build_histogram();
  sc->reorder_data();

  if (sc->next_elemsize) {

    // At this point the input array is already partially sorted, and the
    // elements that remain to be sorted are collected into contiguous
    // chunks. For example if `shift` is 2, then `next_x` may be:
    //     na na | 0 2 1 3 1 | 2 | 1 1 3 0 | 3 0 0 | 2 2 2 2 2 2
    // For each distinct radix there is a "range" within `next_x` that
    // contains values corresponding to that radix. The values in `next_x`
    // have their most significant bits already removed, since they are
    // constant within each radix range. The array `next_x` is accompanied
    // by array `next_o` which carries the original row numbers of each
    // value. Once we sort `next_o` by the values of `next_x` within each
    // radix range, our job will be complete.

    // Prepare the "next SortContext" variable
    size_t strstart = sc->strstart + 1;
    size_t nradixes = sc->nradixes;
    size_t elemsize = (size_t) sc->next_elemsize;
    int8_t nsigbits = sc->shift? sc->shift : sc->next_elemsize * 8;
    int8_t ne = (int8_t)(sc->strdata? sc->next_elemsize :
                         sc->shift > 32? 4 :
                         sc->shift > 16? 2 : 0);

    SortContext next_sc;
    next_sc.strdata = sc->strdata;
    next_sc.stroffs = sc->stroffs;
    next_sc.elemsize = sc->next_elemsize;
    next_sc.nsigbits = nsigbits;
    next_sc.histogram = sc->histogram;  // reuse the `histogram` buffer
    next_sc.next_x = sc->next_x;
    next_sc.next_elemsize = ne;
    next_sc.use_order = sc->use_order;

    // First, determine the sizes of ranges corresponding to each radix that
    // remain to be sorted. Recall that the previous step left us with the
    // `histogram` array containing cumulative sizes of these ranges, so all
    // we need is to diff that array.
    size_t* rrendoffsets = sc->histogram + (sc->nchunks - 1) * nradixes;
    radix_range* rrmap = new radix_range[nradixes];
    for (size_t i = 0; i < nradixes; i++) {
      size_t start = i? rrendoffsets[i-1] : 0;
      size_t end = rrendoffsets[i];
      assert(start <= end);
      rrmap[i].size = end - start;
      rrmap[i].offset = start;
    }

    // Sort the radix ranges in the decreasing size order. This is
    // beneficial because processing large groups first and small groups
    // later reduces the amount of time wasted by threads (for example,
    // suppose there are 2 threads and radix ranges have sizes 1M, 1M, 1M,
    // ..., 10M. Then if the groups are processed in this order, the two
    // threads will first do all 1M chunks finishing simultaneously,
    // then the last thread will be doing 10M chunk while the other thread
    // is idle. Working in the opposite direction, one thread will start
    // with 10M chunk, and the other thread will finish all 1M chunks at
    // the same time, minimizing time wasted).
    qsort(rrmap, nradixes, sizeof(radix_range), _rrcmp);
    assert(rrmap[0].size >= rrmap[nradixes - 1].size);

    // At this point the distribution of radix range sizes may or may not
    // be uniform. If the distribution is uniform (i.e. roughly same number
    // of elements in each range), then the best way to proceed is to let
    // all threads process the ranges in-parallel, each thread working on
    // its own range. However if the distribution is highly skewed (i.e.
    // one or several overly large ranges), then such approach will be
    // suboptimal. In the worst-case scenario we would have single thread
    // processing almost the entire array while other threads are idling.
    // In order to combat such "skew", we first process all "large" radix
    // ranges (they are at the beginning of `rrmap`) one-at-a-time and
    // sorting each of them in a multithreaded way.
    // How big should a radix range be to be considered "large"? If there
    // are `n` elements in the array, and the array is split into `k` ranges
    // then the lower bound for the size of the largest range is `n/k`. In
    // fact, if the largest range's size is exactly `n/k`, then all other
    // ranges are forced to have the same sizes (which is an ideal
    // situation). In practice we deem a range to be "large" if its size is
    // more then `2n/k`.
    size_t rri = 0;
    // size_t rrlarge = 2 * sc->n / nradixes;
    size_t rrlarge = INSERT_SORT_THRESHOLD;  // for now
    while (rrmap[rri].size > rrlarge && rri < nradixes) {
      size_t off = rrmap[rri].offset;
      next_sc.x = add_ptr(sc->x, off * elemsize);
      next_sc.next_x = add_ptr(sc->next_x, off * elemsize);
      next_sc.o = sc->o + off;
      next_sc.n = rrmap[rri].size;
      next_sc.next_elemsize = ne;
      next_sc.strstart = strstart;
      radix_psort(&next_sc);
      rri++;
    }

    // Finally iterate over all remaining radix ranges, in-parallel, and
    // sort each of them independently using a simpler insertion sort
    // method.
    size_t size0 = rri < nradixes? rrmap[rri].size : 0;
    int32_t *tmp = nullptr;
    bool own_tmp = false;
    if (size0) {
      // size_t size_all = size0 * sc->nth * sizeof(int32_t);
      // if ((size_t)sc->elemsize * sc->n <= size_all) {
      //   tmp = (int32_t*)sc->x;
      // } else {
      own_tmp = true;
      dtmalloc_g(tmp, int32_t, size0 * sc->nth);
      // }
    }
    #pragma omp parallel for schedule(dynamic) num_threads(sc->nth)
    for (size_t i = rri; i < nradixes; i++) {
      int me = omp_get_thread_num();
      size_t off = rrmap[i].offset;
      int32_t n = (int32_t) rrmap[i].size;
      if (n <= 1) continue;
      void* x = add_ptr(sc->x, off * elemsize);
      int32_t* o = sc->o + off;
      int32_t* oo = tmp + me * (int32_t)size0;

      if (sc->strdata) {
        int32_t ss = (int32_t) strstart;
        insert_sort_keys_str(sc->strdata, sc->stroffs, ss, o, oo, n);
      } else {
        switch (elemsize) {
          case 1: insert_sort_keys_fw<>(static_cast<uint8_t*>(x), o, oo, n); break;
          case 2: insert_sort_keys_fw<>(static_cast<uint16_t*>(x), o, oo, n); break;
          case 4: insert_sort_keys_fw<>(static_cast<uint32_t*>(x), o, oo, n); break;
          case 8: insert_sort_keys_fw<>(static_cast<uint64_t*>(x), o, oo, n); break;
        }
      }
    }
    delete[] rrmap;
    if (own_tmp) dtfree(tmp);
  }

  // Done. Save to array `o` the computed ordering of the input vector `x`.
  if (ores && sc->o != ores) {
    std::memcpy(ores, sc->o, sc->n * sizeof(int32_t));
    sc->next_o = sc->o;
    sc->o = ores;
  }

  return;
  fail:
  sc->x = nullptr;
  sc->o = nullptr;
}



//==============================================================================
// Insertion sort functions
//==============================================================================

static void insert_sort(SortContext* sc) {
  void*    x = sc->x;
  int32_t* o = sc->o;
  int32_t  n = static_cast<int32_t>(sc->n);
  if (sc->use_order) {
    arr32_t tmparr(sc->n);
    int32_t* t = tmparr.data();
    if (sc->strdata) {
      insert_sort_keys_str(sc->strdata, sc->stroffs, 0, o, t, n);
    } else {
      switch (sc->elemsize) {
        case 1: insert_sort_keys_fw(static_cast<uint8_t* >(x), o, t, n); break;
        case 2: insert_sort_keys_fw(static_cast<uint16_t*>(x), o, t, n); break;
        case 4: insert_sort_keys_fw(static_cast<uint32_t*>(x), o, t, n); break;
        case 8: insert_sort_keys_fw(static_cast<uint64_t*>(x), o, t, n); break;
      }
    }
  } else {
    if (sc->strdata) {
      insert_sort_values_str(sc->strdata, sc->stroffs, 0, o, n);
    } else {
      switch (sc->elemsize) {
        case 1: insert_sort_values_fw(static_cast<uint8_t*>(x),  o, n); break;
        case 2: insert_sort_values_fw(static_cast<uint16_t*>(x), o, n); break;
        case 4: insert_sort_values_fw(static_cast<uint32_t*>(x), o, n); break;
        case 8: insert_sort_values_fw(static_cast<uint64_t*>(x), o, n); break;
      }
    }
  }
}
