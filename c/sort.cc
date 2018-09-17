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
// is sorted in ascending order. The sorting is stable, and will gather all NA
// values in `x` (if any) at the beginning of the sorted list.
//
// See also:
//      https://en.wikipedia.org/wiki/Radix_sort
//      https://en.wikipedia.org/wiki/Insertion_sort
//      http://stereopsis.com/radix.html
//      /microbench/sort
//
// Based on the parallel radix sort algorithm by Matt Dowle in (R)data.table:
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
//    The core of the MSB Radix Sort algorithm is that we look at first
//    `nradixbits` most significant bits of each number (these bits are called
//    the "radix"), and sort the input according to those prefixes.
//
//    In this step we compute the frequency table (or histogram) of the radixes
//    of each element in the input array `x`. The histogram is computed
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
//    significant `nradixbits` (or for strings, by their first characters), and
//    the values in `x` are properly transformed for further sorting. Also, the
//    `histogram` matrix from step 2 carries information about where each
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
#include <vector>     // std::vector
#include "column.h"
#include "datatable.h"
#include "options.h"
#include "rowindex.h"
#include "types.h"
#include "utils.h"
#include "utils/alloc.h"
#include "utils/array.h"
#include "utils/assert.h"
#include "utils/omp.h"

//------------------------------------------------------------------------------
// Helper classes for managing memory
//------------------------------------------------------------------------------

class omem {
  private:
    void* ptr;
    size_t size;
  public:
    omem();
    ~omem();
    void ensure_size(size_t n);
    friend class rmem;
};

class rmem {
  private:
    void* ptr;
    size_t size;
  public:
    rmem();
    rmem(const rmem&);
    rmem(const rmem&, size_t offset, size_t n);
    rmem& operator=(const rmem&) = default;
    rmem& operator=(const omem&);
    explicit operator bool() const noexcept;
    template <typename T> T* data() const noexcept;
    friend void swap(rmem& left, rmem& right) noexcept;
};


//------------------------------------------------------------------------------

omem::omem() : ptr(nullptr), size(0) {}

omem::~omem() {
  dt::free(ptr);
}

void omem::ensure_size(size_t n) {
  if (n <= size) return;
  ptr = dt::realloc(ptr, n);  // throws an exception if cannot allocate
  size = n;
}

rmem::rmem() : ptr(nullptr), size(0) {}


rmem::rmem(const rmem& o) : ptr(o.ptr), size(o.size) {}

rmem::rmem(const rmem& o, size_t offset, size_t n)
    : ptr(static_cast<char*>(o.ptr) + offset), size(n)
{
  xassert(o.size <= offset + n);
}

rmem& rmem::operator=(const omem& o) {
  ptr = o.ptr;
  size = o.size;
  return *this;
}

rmem::operator bool() const noexcept {
  return (ptr != nullptr);
}

template <typename T> T* rmem::data() const noexcept {
  return static_cast<T*>(ptr);
}

void swap(rmem& left, rmem& right) noexcept {
  std::swap(left.ptr, right.ptr);
  std::swap(left.size, right.size);
}



//------------------------------------------------------------------------------
// SortContext
//------------------------------------------------------------------------------

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
 * strtype
 *   0 when sorting non-strings, 1 if sorting str32, 2 if sorting str64
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
  private:
    arr32_t order;
    arr32_t groups;
    MemoryRange mr_x;
    MemoryRange mr_xx;
    dt::array<size_t> arr_hist;
    GroupGatherer gg;

    void* x;
    void* next_x;
    int32_t* o;
    int32_t* next_o;
    size_t*  histogram;
    const uint8_t* strdata;
    const void* stroffs;
    size_t strstart;
    size_t n;
    size_t nth;
    size_t nchunks;
    size_t chunklen;
    size_t nradixes;
    uint8_t elemsize;
    uint8_t next_elemsize;
    int8_t nsigbits;
    int8_t shift;
    int8_t strtype;
    bool use_order;
    int : 16;

  public:
  SortContext(size_t nrows, const RowIndex& rowindex, bool make_groups) {
    x = nullptr;
    next_x = nullptr;
    next_o = nullptr;
    strdata = nullptr;

    nth = static_cast<size_t>(config::sort_nthreads);
    n = nrows;
    order = rowindex.extract_as_array32();
    use_order = static_cast<bool>(order);
    if (!use_order) order.resize(n);
    o = order.data();
    if (make_groups) {
      groups.resize(n + 1);
      groups[0] = 0;
      gg.init(groups.data() + 1, 0);
    }
  }

  SortContext(const SortContext&) = delete;
  SortContext& operator=(const SortContext&) = delete;

  ~SortContext() {
    // Note: `o` is not owned by this class, see `initialize()`
    delete[] next_o;
  }


  void start_sort(const Column* col) {
    _prepare_data_for_column(col, true);
    if (n <= config::sort_insert_method_threshold) {
      if (use_order) {
        kinsert_sort();
      } else {
        vinsert_sort();
      }
    } else {
      radix_psort();
    }
  }


  void continue_sort(const Column* col, bool make_groups) {
    nradixes = static_cast<size_t>(gg.size());
    use_order = true;
    xassert(nradixes > 0);
    xassert(o == order.data());
    _prepare_data_for_column(col, false);
    dt::array<radix_range> rrmap(nradixes);
    radix_range* rrmap_ptr = rrmap.data();
    _fill_rrmap_from_groups(rrmap_ptr);
    if (make_groups) {
      gg.init(groups.data() + 1, 0);
      _radix_recurse<true>(rrmap_ptr);
    } else {
      _radix_recurse<false>(rrmap_ptr);
    }
  }


  RowIndex get_result(Groupby* out_grps) {
    RowIndex res = RowIndex::from_array32(std::move(order));
    if (out_grps) {
      size_t ngrps = static_cast<size_t>(gg.size());
      xassert(groups.size() > ngrps);
      groups.resize(ngrps + 1);
      *out_grps = Groupby(ngrps, groups.to_memoryrange());
    }
    return res;
  }



  //============================================================================
  // Data preparation
  //============================================================================

  template <typename T>
  T* _allocate_and_get_x() {
    size_t nbytes = n * sizeof(T);
    if (mr_x.size() < nbytes) {
      mr_x.resize(nbytes, /* keep_data = */ false);
    }
    elemsize = sizeof(T);
    x = mr_x.xptr();
    return static_cast<T*>(x);
  }

  void _allocate_next_x(size_t nbytes) {
    if (mr_xx.size() < nbytes) {
      mr_xx.resize(nbytes, /* keep_data = */ false);
    }
    next_x = mr_xx.xptr();
  }

  void _prepare_data_for_column(const Column* col, bool /*firstcol*/) {
    strtype = 0;
    // These will initialize `x`, `elemsize` and `nsigbits`, and also
    // `strdata`, `stroffs`, `strstart` for string columns
    SType stype = col->stype();
    switch (stype) {
      case SType::BOOL:    _initB(col); break;
      case SType::INT8:    _initI<int8_t,  uint8_t>(col); break;
      case SType::INT16:   _initI<int16_t, uint16_t>(col); break;
      case SType::INT32:   _initI<int32_t, uint32_t>(col); break;
      case SType::INT64:   _initI<int64_t, uint64_t>(col); break;
      case SType::FLOAT32: _initF<uint32_t>(col); break;
      case SType::FLOAT64: _initF<uint64_t>(col); break;
      case SType::STR32:
      case SType::STR64: {
        if (stype == SType::STR32) _initS<uint32_t>(col);
        else                       _initS<uint64_t>(col);
        // if (!firstcol) {
        //   strstart = size_t(-1);
        // }
        break;
      }
      default:
        throw NotImplError() << "Unable to sort Column of stype " << stype;
    }
  }


  /**
   * Boolean columns have only 3 distinct values: -128, 0 and 1. The transform
   * `(x + 0xBF) >> 6` converts these to 0, 2 and 3 respectively, provided that
   * the addition is done as addition of unsigned bytes (i.e. modulo 256).
   *
   * transform      | -128  0   1
   * (x + 191) >> 6 |   0   2   3   NA first, ASC
   * (x + 63) >> 6  |   2   0   1   NA last,  ASC
   * (64 - x) >> 6  |   3   1   0   NA first, DESC
   * (128 - x) >> 6 |   0   2   1   NA last,  DESC
   */
  void _initB(const Column* col) {
    const uint8_t* xi = static_cast<const uint8_t*>(col->data());
    uint8_t* xo = _allocate_and_get_x<uint8_t>();
    nsigbits = 2;

    if (use_order) {
      #pragma omp parallel for schedule(static) num_threads(nth)
      for (size_t j = 0; j < n; j++) {
        uint8_t t = xi[o[j]] + 0xBF;
        xo[j] = t >> 6;
      }
    } else {
      #pragma omp parallel for schedule(static) num_threads(nth)
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
    xassert(sizeof(T) == sizeof(TU));
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
    const TI* xi = static_cast<const TI*>(col->data());
    TO* xo = _allocate_and_get_x<TO>();

    if (use_order) {
      #pragma omp parallel for schedule(static) num_threads(nth)
      for (size_t j = 0; j < n; ++j) {
        TI t = xi[o[j]];
        xo[j] = t == una? 0 : static_cast<TO>(t - umin + 1);
      }
    } else {
      #pragma omp parallel for schedule(static) num_threads(nth)
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
    const TO* xi = static_cast<const TO*>(col->data());
    TO* xo = _allocate_and_get_x<TO>();
    nsigbits = static_cast<int8_t>(elemsize * 8);

    constexpr TO EXP
      = static_cast<TO>(sizeof(TO) == 8? 0x7FF0000000000000ULL : 0x7F800000);
    constexpr TO SIG
      = static_cast<TO>(sizeof(TO) == 8? 0x000FFFFFFFFFFFFFULL : 0x007FFFFF);
    constexpr TO SBT
      = static_cast<TO>(sizeof(TO) == 8? 0x8000000000000000ULL : 0x80000000);
    constexpr int SHIFT = sizeof(TO) * 8 - 1;

    if (use_order) {
      #pragma omp parallel for schedule(static) num_threads(nth)
      for (size_t j = 0; j < n; j++) {
        TO t = xi[o[j]];
        xo[j] = ((t & EXP) == EXP && (t & SIG) != 0)
                ? 0 : t ^ (SBT | -(t>>SHIFT));
      }
    } else {
      #pragma omp parallel for schedule(static) num_threads(nth)
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
   * `strstart` and `strtype`.
   *
   * More specifically, for each string item, if it is NA then we map it to 0;
   * if it is an empty string we map it to 1, otherwise we map it to `ch[i] + 2`
   * where `ch[i]` is the i-th character of the string. This doesn't overflow
   * because in UTF-8 the largest legal byte is 0xF7.
   */
  template <typename T>
  void _initS(const Column* col) {
    auto scol = static_cast<const StringColumn<T>*>(col);
    strdata = reinterpret_cast<const uint8_t*>(scol->strdata());
    const T* offs = scol->offsets();
    stroffs = static_cast<const void*>(offs);
    uint8_t* xo = _allocate_and_get_x<uint8_t>();
    strtype = sizeof(T) / 4;
    strstart = 0;
    nsigbits = 8;

    T maxlen = 0;
    #pragma omp parallel for schedule(static) num_threads(nth) \
            reduction(max:maxlen)
    for (size_t j = 0; j < n; ++j) {
      int32_t k = use_order? o[j] : static_cast<int32_t>(j);
      T offend = offs[k];
      if (ISNA<T>(offend)) {
        xo[j] = 0;    // NA string
      } else {
        T offstart = offs[k - 1] & ~GETNA<T>();
        if (offend > offstart) {
          xo[j] = strdata[offstart] + 2;
          T len = offend - offstart;
          if (len > maxlen) maxlen = len;
        } else {
          xo[j] = 1;  // empty string
        }
      }
    }
    next_elemsize = (maxlen > 1);
  }


  //============================================================================
  // Radix sorting parameters
  //============================================================================

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
  void determine_sorting_parameters() {
    size_t nch = nth * config::sort_thread_multiplier;
    chunklen = std::max((n - 1) / nch + 1,
                        config::sort_max_chunk_length);
    nchunks = (n - 1)/chunklen + 1;

    int8_t nradixbits = nsigbits < config::sort_max_radix_bits
                        ? nsigbits : config::sort_over_radix_bits;
    shift = nsigbits - nradixbits;
    nradixes = 1 << nradixbits;

    if (!strdata) {
      // The remaining number of sig.bits is `shift`. Thus, this value will
      // determine the `next_elemsize`.
      next_elemsize = shift > 32? 8 :
                      shift > 16? 4 :
                      shift > 0? 2 : 0;
    }
  }



  //============================================================================
  // Histograms (for radix sort)
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
    arr_hist.ensuresize(counts_size);
    histogram = arr_hist.data();
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
  // Reorder data (for radix sort)
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
      _allocate_next_x(n * next_elemsize);
    }
    if (!next_o) {
      next_o = new int32_t[n];
    }
    if (strtype) {
      if (next_x) {
        if (strtype == 1) _reorder_str<uint32_t>();
        else              _reorder_str<uint64_t>();
      } else _reorder_impl<uint8_t, char, false>();
    } else {
      switch (elemsize) {
        case 8:
          xassert(next_elemsize == 8 || next_elemsize == 4);
          if (next_elemsize == 8) _reorder_impl<uint64_t, uint64_t, true>();
          if (next_elemsize == 4) _reorder_impl<uint64_t, uint32_t, true>();
          break;
        case 4:
          xassert(next_elemsize == 4 || next_elemsize == 2);
          if (next_elemsize == 4) _reorder_impl<uint32_t, uint32_t, true>();
          if (next_elemsize == 2) _reorder_impl<uint32_t, uint16_t, true>();
          break;
        case 2:
          xassert(next_elemsize == 2 || next_elemsize == 0);
          if (next_elemsize == 2) _reorder_impl<uint16_t, uint16_t, true>();
          if (next_elemsize == 0) _reorder_impl<uint16_t, uint8_t, false>();
          break;
        case 1:
          xassert(next_elemsize == 0);
          _reorder_impl<uint8_t, uint8_t, false>();
          break;
      }
    }
    std::swap(x, next_x);
    std::swap(o, next_o);
    std::swap(elemsize, next_elemsize);
    use_order = true;
  }

  template<typename TI, typename TO, bool OUT> void _reorder_impl() {
    TI* xi = static_cast<TI*>(x);
    TO* xo;
    TI mask;
    if (OUT) {
      xo = static_cast<TO*>(next_x);
      mask = static_cast<TI>((1ULL << shift) - 1);
    }
    #pragma omp parallel for schedule(dynamic) num_threads(nth)
    for (size_t i = 0; i < nchunks; ++i) {
      size_t j0 = i * chunklen;
      size_t j1 = std::min(j0 + chunklen, n);
      size_t* tcounts = histogram + (nradixes * i);
      for (size_t j = j0; j < j1; ++j) {
        size_t k = tcounts[xi[j] >> shift]++;
        xassert(k < n);
        next_o[k] = use_order? o[j] : static_cast<int32_t>(j);
        if (OUT) {
          xo[k] = static_cast<TO>(xi[j] & mask);
        }
      }
    }
    xassert(histogram[nchunks * nradixes - 1] == n);
  }

  template <typename T> void _reorder_str() {
    uint8_t* xi = static_cast<uint8_t*>(x);
    uint8_t* xo = static_cast<uint8_t*>(next_x);
    const T sstart = static_cast<T>(strstart) + 1;
    const T* soffs = static_cast<const T*>(stroffs);

    T maxlen = 0;
    #pragma omp parallel for schedule(dynamic) num_threads(nth) \
            reduction(max:maxlen)
    for (size_t i = 0; i < nchunks; ++i) {
      size_t j0 = i * chunklen;
      size_t j1 = std::min(j0 + chunklen, n);
      size_t* tcounts = histogram + (nradixes * i);
      for (size_t j = j0; j < j1; ++j) {
        size_t k = tcounts[xi[j]]++;
        xassert(k < n);
        int32_t w = use_order? o[j] : static_cast<int32_t>(j);
        T offend = soffs[w];
        T offstart = (soffs[w - 1] & ~GETNA<T>()) + sstart;
        if (ISNA<T>(offend)) {
          xo[k] = 0;
        } else if (offend > offstart) {
          xo[k] = strdata[offstart] + 2;
          T len = offend - offstart;
          if (len > maxlen) maxlen = len;
        } else {
          xo[k] = 1;  // string is shorter than sstart
        }
        next_o[k] = w;
      }
    }
    next_elemsize = (maxlen > 0);
    xassert(histogram[nchunks * nradixes - 1] == n);
  }



  //============================================================================
  // Radix sort
  //============================================================================

  /**
   * Sort array `o` of length `n` by values `x`, using MSB Radix sort algorithm.
   * Array `o` will be modified in-place. If `o` is nullptr, then it will be
   * allocated and filled with values of `range(n)`.
   */
  void radix_psort() {
    int32_t* ores = o;
    determine_sorting_parameters();
    build_histogram();
    reorder_data();

    if (elemsize) {
      // If after reordering there are still unsorted elements in `x`, then
      // sort them recursively.
      int8_t _nsigbits = nsigbits;
      nsigbits = strdata? 8 : shift;
      dt::array<radix_range> rrmap(nradixes);
      radix_range* rrmap_ptr = rrmap.data();
      _fill_rrmap_from_histogram(rrmap_ptr);
      if (groups) _radix_recurse<true>(rrmap_ptr);
      else        _radix_recurse<false>(rrmap_ptr);
      nsigbits = _nsigbits;
    }
    else if (groups) {
      // Otherwise groups can be computed directly from the histogram
      gg.from_histogram(histogram, nchunks, nradixes);
    }

    // Done. Save to array `o` the computed ordering of the input vector `x`.
    if (ores && o != ores) {
      std::memcpy(ores, o, n * sizeof(int32_t));
      next_o = o;
      o = ores;
    }
  }

  void _fill_rrmap_from_histogram(radix_range* rrmap) {
    // First, determine the sizes of ranges corresponding to each radix that
    // remain to be sorted. The previous step left us with the `histogram`
    // array containing cumulative sizes of these ranges, so all we need is
    // to diff that array.
    size_t* rrendoffsets = histogram + (nchunks - 1) * nradixes;
    for (size_t i = 0; i < nradixes; i++) {
      size_t start = i? rrendoffsets[i-1] : 0;
      size_t end = rrendoffsets[i];
      xassert(start <= end);
      rrmap[i].size   = end - start;
      rrmap[i].offset = start;
    }
  }

  void _fill_rrmap_from_groups(radix_range* rrmap) {
    size_t ng = static_cast<size_t>(gg.size());
    for (size_t i = 0; i < ng; ++i) {
      rrmap[i].offset = static_cast<size_t>(groups[i]);
      rrmap[i].size   = static_cast<size_t>(groups[i + 1] - groups[i]);
    }
  }

  /**
   * Helper for radix sorting function.
   *
   * At this point the input array is already partially sorted, and the
   * elements that remain to be sorted are collected into contiguous
   * chunks. For example if `shift` is 2, then `x` may be:
   *     na na | 0 2 1 3 1 | 2 | 1 1 3 0 | 3 0 0 | 2 2 2 2 2 2
   *
   * For each distinct radix there is a "range" within `x` that
   * contains values corresponding to that radix. The values in `x`
   * have their most significant bits already removed, since they are
   * constant within each radix range. The array `x` is accompanied
   * by array `o` which carries the original row numbers of each
   * value. Once we sort `o` by the values of `x` within each
   * radix range, our job will be complete.
   */
  template <bool make_groups>
  void _radix_recurse(radix_range* rrmap) {
    // Save some of the variables in SortContext that we will be modifying
    // in order to perform the recursion.
    size_t   _n        = n;
    void*    _x        = x;
    void*    _next_x   = next_x;
    int32_t* _o        = o;
    int32_t* _next_o   = next_o;
    uint8_t  _elemsize = elemsize;
    size_t   _nradixes = nradixes;
    size_t   _strstart = strstart;
    int32_t  ggoff0    = make_groups? gg.cumulative_size() : 0;
    int32_t* ggdata0   = make_groups? gg.data() : nullptr;

    // At this point the distribution of radix range sizes may or may not
    // be uniform. If the distribution is uniform (i.e. roughly same number
    // of elements in each range), then the best way to proceed is to let
    // all threads process the ranges in-parallel, each thread working on
    // its own range. However if the distribution is highly skewed (i.e.
    // one or several overly large ranges), then such approach will be
    // suboptimal. In the worst-case scenario we would have single thread
    // processing almost the entire array while other threads are idling.
    // In order to combat such "skew", we first process all "large" radix
    // ranges one-at-a-time and sorting each of them in a multithreaded way.
    // How big should a radix range be to be considered "large"? If there
    // are `n` elements in the array, and the array is split into `k` ranges
    // then the lower bound for the size of the largest range is `n/k`. In
    // fact, if the largest range's size is exactly `n/k`, then all other
    // ranges are forced to have the same sizes (which is an ideal
    // situation). In practice we deem a range to be "large" if its size is
    // more then `2n/k`.
    // size_t rrlarge = 2 * _n / nradixes;
    constexpr size_t GROUPED = size_t(1) << 63;
    size_t size0 = 0;
    size_t nsmallgroups = 0;
    size_t rrlarge = config::sort_insert_method_threshold;  // for now
    xassert(GROUPED > rrlarge);

    strstart = _strstart + 1;

    for (size_t rri = 0; rri < _nradixes; ++rri) {
      size_t sz = rrmap[rri].size;
      if (sz > rrlarge) {
        size_t off = rrmap[rri].offset;
        elemsize = _elemsize;
        n = sz;
        x = static_cast<void*>(static_cast<char*>(_x) + off * elemsize);
        o = _o + off;
        next_x = static_cast<void*>(
                    static_cast<char*>(_next_x) + off * elemsize);
        next_o = _next_o + off;
        if (make_groups) {
          gg.init(ggdata0 + off, ggoff0 + static_cast<int32_t>(off));
        }
        radix_psort();
        if (make_groups) {
          rrmap[rri].size = static_cast<size_t>(gg.size()) | GROUPED;
        }
      } else {
        nsmallgroups += (sz > 1);
        if (sz > size0) size0 = sz;
      }
    }

    n = _n;
    x = _x;
    o = _o;
    next_x = _next_x;
    next_o = _next_o;
    strstart = _strstart;
    elemsize = _elemsize;
    gg.init(ggdata0, ggoff0);

    // Finally iterate over all remaining radix ranges, in-parallel, and
    // sort each of them independently using a simpler insertion sort
    // method.
    size_t nthreads = std::min(nth, nsmallgroups);
    int32_t* tmp = nullptr;
    bool own_tmp = false;
    if (size0) {
      // size_t size_all = size0 * nthreads * sizeof(int32_t);
      // if ((size_t)_next_elemsize * _n <= size_all) {
      //   tmp = (int32_t*)_x;
      // } else {
      own_tmp = true;
      tmp = new int32_t[size0 * nthreads];
      // }
    }
    #pragma omp parallel num_threads(nthreads)
    {
      int tnum = omp_get_thread_num();
      int32_t* oo = tmp + tnum * static_cast<int32_t>(size0);
      GroupGatherer tgg;

      #pragma omp for schedule(dynamic)
      for (size_t i = 0; i < _nradixes; ++i) {
        size_t zn  = rrmap[i].size;
        size_t off = rrmap[i].offset;
        if (zn > rrlarge) {
          rrmap[i].size = zn & ~GROUPED;
        } else if (zn > 1) {
          int32_t  tn = static_cast<int32_t>(zn);
          void*    tx = static_cast<char*>(_x) + off * elemsize;
          int32_t* to = _o + off;
          if (make_groups) {
            tgg.init(ggdata0 + off, static_cast<int32_t>(off) + ggoff0);
          }
          if (strtype == 0) {
            switch (elemsize) {
              case 1: insert_sort_keys<>(static_cast<uint8_t*>(tx), to, oo, tn, tgg); break;
              case 2: insert_sort_keys<>(static_cast<uint16_t*>(tx), to, oo, tn, tgg); break;
              case 4: insert_sort_keys<>(static_cast<uint32_t*>(tx), to, oo, tn, tgg); break;
              case 8: insert_sort_keys<>(static_cast<uint64_t*>(tx), to, oo, tn, tgg); break;
            }
          } else if (strtype == 1) {
            const uint32_t* soffs = static_cast<const uint32_t*>(stroffs);
            uint32_t ss = static_cast<uint32_t>(_strstart + 1);
            insert_sort_keys_str(strdata, soffs, ss, to, oo, tn, tgg);
          } else {
            const uint64_t* soffs = static_cast<const uint64_t*>(stroffs);
            uint64_t ss = static_cast<uint64_t>(_strstart + 1);
            insert_sort_keys_str(strdata, soffs, ss, to, oo, tn, tgg);
          }
          if (make_groups) {
            rrmap[i].size = static_cast<size_t>(tgg.size());
          }
        } else if (zn == 1 && make_groups) {
          ggdata0[off] = static_cast<int32_t>(off) + ggoff0 + 1;
          rrmap[i].size = 1;
        }
      }
    }

    // Consolidate groups into a single contiguous chunk
    if (make_groups) {
      gg.from_chunks(rrmap, _nradixes);
    }

    if (own_tmp) delete[] tmp;
  }



  //============================================================================
  // Insert sort
  //============================================================================

  void kinsert_sort() {
    arr32_t tmparr(n);
    int32_t* tmp = tmparr.data();
    int32_t nn = static_cast<int32_t>(n);
    if (strtype == 0) {
      switch (elemsize) {
        case 1: _insert_sort_keys<uint8_t >(tmp); break;
        case 2: _insert_sort_keys<uint16_t>(tmp); break;
        case 4: _insert_sort_keys<uint32_t>(tmp); break;
        case 8: _insert_sort_keys<uint64_t>(tmp); break;
      }
    } else if (strtype == 1) {
      const uint32_t* soffs = static_cast<const uint32_t*>(stroffs);
      insert_sort_keys_str(strdata, soffs, uint32_t(0), o, tmp, nn, gg);
    } else {
      const uint64_t* soffs = static_cast<const uint64_t*>(stroffs);
      insert_sort_keys_str(strdata, soffs, uint64_t(0), o, tmp, nn, gg);
    }
  }

  void vinsert_sort() {
    if (strtype == 0) {
      switch (elemsize) {
        case 1: _insert_sort_values<uint8_t >(); break;
        case 2: _insert_sort_values<uint16_t>(); break;
        case 4: _insert_sort_values<uint32_t>(); break;
        case 8: _insert_sort_values<uint64_t>(); break;
      }
    } else if (strtype == 1) {
      int32_t nn = static_cast<int32_t>(n);
      const uint32_t* soffs = static_cast<const uint32_t*>(stroffs);
      insert_sort_values_str(strdata, soffs, uint32_t(0), o, nn, gg);
    } else {
      int32_t nn = static_cast<int32_t>(n);
      const uint64_t* soffs = static_cast<const uint64_t*>(stroffs);
      insert_sort_values_str(strdata, soffs, uint64_t(0), o, nn, gg);
    }
  }

  template <typename T> void _insert_sort_keys(int32_t* tmp) {
    T* xt = static_cast<T*>(x);
    int32_t nn = static_cast<int32_t>(n);
    insert_sort_keys(xt, o, tmp, nn, gg);
  }

  template <typename T> void _insert_sort_values() {
    T* xt = static_cast<T*>(x);
    int32_t nn = static_cast<int32_t>(n);
    insert_sort_values(xt, o, nn, gg);
  }

};



//==============================================================================
// Main sorting routines
//==============================================================================

/**
 * Sort the column, and return its ordering as a RowIndex object. This function
 * will choose the most appropriate algorithm for sorting. The data in column
 * `col` will not be modified.
 *
 * The function returns nullptr if there is a runtime error (for example an
 * intermediate buffer cannot be allocated).
 */
RowIndex DataTable::sortby(const arr32_t& colindices, Groupby* out_grps) const
{
  size_t nsortcols = colindices.size();
  if (nrows > INT32_MAX) {
    throw NotImplError() << "Cannot sort a datatable with " << nrows << " rows";
  }
  if (rowindex.isarr64() || rowindex.length() > INT32_MAX ||
      rowindex.max() > INT32_MAX) {
    throw NotImplError() << "Cannot sort a datatable which is based on a "
                            "datatable with >2**31 rows";
  }
  // TODO: fix for the multi-rowindex case (#1188)
  // A frame can be sorted by columns col1, ..., colN if and only if all these
  // columns have the same rowindex. The resulting RowIndex can be applied to
  // to all columns in the frame iff one of the following holds: (1) all
  // columns in the frame has the same rowindex; or (2) the sortby columns
  // do not have any rowindex.
  Column* col0 = columns[colindices[0]];
  for (size_t i = 1; i < nsortcols; ++i) {
    if (columns[colindices[i]]->rowindex() != col0->rowindex()) {
      for (size_t j = 0; j < nsortcols; ++j) {
        columns[colindices[j]]->reify();
      }
      break;
    }
  }

  if (nrows <= 1) {
    int64_t i = col0->rowindex().nth(0);
    if (out_grps) {
      *out_grps = Groupby::single_group(col0->nrows);
    }
    return RowIndex::from_slice(i, col0->nrows, 1);
  }
  size_t zrows = static_cast<size_t>(nrows);
  SortContext sc(zrows, col0->rowindex(),
                 (out_grps != nullptr) || (nsortcols > 1));
  sc.start_sort(col0);
  for (size_t j = 1; j < nsortcols; ++j) {
    sc.continue_sort(columns[colindices[j]],
                     (out_grps != nullptr) || (j < nsortcols - 1));
  }
  return sc.get_result(out_grps);
}


static RowIndex sort_tiny(const Column* col, Groupby* out_grps) {
  int64_t i = col->rowindex().nth(0);
  if (out_grps) {
    *out_grps = Groupby::single_group(col->nrows);
  }
  return RowIndex::from_slice(i, col->nrows, 1);
}


RowIndex Column::sort(Groupby* out_grps) const {
  if (nrows <= 1) {
    return sort_tiny(this, out_grps);
  }
  size_t zrows = static_cast<size_t>(nrows);
  SortContext sc(zrows, rowindex(), (out_grps != nullptr));
  sc.start_sort(this);
  return sc.get_result(out_grps);
}
