//------------------------------------------------------------------------------
// Copyright 2018-2019 H2O.ai
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
#include <algorithm>  // std::min
#include <atomic>     // std::atomic_flag
#include <cstdlib>    // std::abs
#include <cstring>    // std::memset, std::memcpy
#include <vector>     // std::vector
#include "column/view.h"
#include "expr/py_sort.h"
#include "expr/eval_context.h"
#include "frame/py_frame.h"
#include "parallel/api.h"
#include "python/args.h"
#include "utils/alloc.h"
#include "utils/array.h"
#include "utils/assert.h"
#include "utils/misc.h"
#include "column.h"
#include "datatable.h"
#include "datatablemodule.h"
#include "options.h"
#include "rowindex.h"
#include "sort.h"
#include "types.h"

//------------------------------------------------------------------------------
// Helper classes for managing memory
//------------------------------------------------------------------------------

class omem {
  private:
  public:
    void* ptr;
    size_t size;
  public:
    omem();
    ~omem();
    void ensure_size(size_t n);
    void* release();
    friend class rmem;
};

class rmem {
  private:
  public:
    void* ptr;
    #ifdef DTDEBUG
      size_t size;
    #endif
  public:
    rmem();
    explicit rmem(const omem&);
    explicit rmem(const rmem&);
    rmem(const rmem&, size_t offset, size_t n);
    rmem& operator=(const rmem&) = default;
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

void* omem::release() {
  void* p = ptr;
  ptr = nullptr;
  return p;
}


rmem::rmem() {
  ptr = nullptr;
  #ifdef DTDEBUG
    size = 0;
  #endif
}

rmem::rmem(const omem& o) {
  ptr = o.ptr;
  #ifdef DTDEBUG
    size = o.size;
  #endif
}

rmem::rmem(const rmem& o) {
  ptr = o.ptr;
  #ifdef DTDEBUG
    size = o.size;
  #endif
}

rmem::rmem(const rmem& o, size_t offset, size_t n) {
  ptr = static_cast<char*>(o.ptr) + offset;
  (void)n;
  #ifdef DTDEBUG
    size = n;
    xassert(offset + n <= o.size);
  #endif
}

rmem::operator bool() const noexcept {
  return (ptr != nullptr);
}

template <typename T> T* rmem::data() const noexcept {
  return static_cast<T*>(ptr);
}

void swap(rmem& left, rmem& right) noexcept {
  std::swap(left.ptr, right.ptr);
  #ifdef DTDEBUG
    std::swap(left.size, right.size);
  #endif
}



//------------------------------------------------------------------------------
// Options
//------------------------------------------------------------------------------

static size_t sort_insert_method_threshold = 64;
static size_t sort_thread_multiplier = 2;
static size_t sort_max_chunk_length = 1 << 8;
static uint8_t sort_max_radix_bits = 12;
static uint8_t sort_over_radix_bits = 8;
static int32_t sort_nthreads = static_cast<int>(dt::get_hardware_concurrency());

void sort_init_options() {
  dt::register_option(
    "sort.insert_method_threshold",
    []{ return py::oint(sort_insert_method_threshold); },
    [](const py::Arg& value) {
      int64_t n = value.to_int64_strict();
      if (n < 0) n = 0;
      sort_insert_method_threshold = static_cast<size_t>(n);
    },
    "Largest n at which sorting will be performed using insert sort\n"
    "method. This setting also governs the recursive parts of the\n"
    "radix sort algorithm, when we need to sort smaller sub-parts of\n"
    "the input.");

  dt::register_option(
    "sort.thread_multiplier",
    []{ return py::oint(sort_thread_multiplier); },
    [](const py::Arg& value) {
      int64_t n = value.to_int64_strict();
      if (n < 1) n = 1;
      sort_thread_multiplier = static_cast<size_t>(n);
    }, "");

  dt::register_option(
    "sort.max_chunk_length",
    []{ return py::oint(sort_max_chunk_length); },
    [](const py::Arg& value) {
      int64_t n = value.to_int64_strict();
      if (n < 1) n = 1;
      sort_max_chunk_length = static_cast<size_t>(n);
    }, "");

  dt::register_option(
    "sort.max_radix_bits",
    []{ return py::oint(sort_max_radix_bits); },
    [](const py::Arg& value) {
      int64_t n = value.to_int64_strict();
      if (n <= 0)
        throw ValueError() << "Invalid sort.max_radix_bits parameter: " << n;
      sort_max_radix_bits = static_cast<uint8_t>(n);
    }, "");

  dt::register_option(
    "sort.over_radix_bits",
    []{ return py::oint(sort_over_radix_bits); },
    [](const py::Arg& value) {
      int64_t n = value.to_int64_strict();
      if (n <= 0)
        throw ValueError() << "Invalid sort.over_radix_bits parameter: " << n;
      sort_over_radix_bits = static_cast<uint8_t>(n);
    }, "");

  dt::register_option(
    "sort.nthreads",
    []{ return py::oint(sort_nthreads); },
    [](const py::Arg& value) {
      int32_t nth = value.to_int32_strict();
      if (nth <= 0) nth += static_cast<int32_t>(dt::get_hardware_concurrency());
      if (nth <= 0) nth = 1;
      sort_nthreads = static_cast<uint8_t>(nth);
    }, "");
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
 * is_string
 *   true when sorting strings, false otherwise
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
 * xx
 *   When `shift > 0`, a single pass of the `radix_psort()` function will sort
 *   the data array only partially, and 1 or more extra passes will be needed to
 *   finish sorting. In this case the array `xx` (of length `n`) will hold
 *   pre-sorted and potentially modified values of the original data array `x`.
 *   The array `xx` is filled in the "reorder_data" step. If it was nullptr,
 *   the array will be allocated, otherwise its contents will be overwritten.
 *
 * next_o
 *   The reordered array `o` to be carried over to the next step, or to be
 *   returned as the final ordering in the end.
 *
 * next_elemsize
 *   Size in bytes of each element in `xx`. This cannot be greater than
 *   `elemsize`, however `next_elemsize` can be 0.
 */
class SortContext {
  private:
    arr32_t groups;
    omem container_x;
    omem container_xx;
    omem container_o;
    omem container_oo;
    dt::array<size_t> arr_hist;
    GroupGatherer gg;

    rmem x;
    rmem xx;
    int32_t* o;
    int32_t* next_o;
    size_t*  histogram;
    Column column;
    size_t strstart;
    size_t n;
    size_t nth;
    size_t nchunks;
    size_t chunklen;
    size_t nradixes;
    uint8_t elemsize;
    uint8_t next_elemsize;
    uint8_t nsigbits;
    uint8_t shift;
    bool is_string;
    bool use_order;
    bool descending;
    int : 8;

  public:
  SortContext(size_t nrows, const RowIndex& rowindex, bool make_groups) {
    o = nullptr;
    next_o = nullptr;
    histogram = nullptr;
    nchunks = 0;
    chunklen = 0;
    nradixes = 0;
    elemsize = 0;
    next_elemsize = 0;
    nsigbits = 0;
    shift = 0;
    is_string = false;
    use_order = false;
    descending = false;

    nth = static_cast<size_t>(sort_nthreads);
    n = nrows;
    container_o.ensure_size(n * sizeof(int32_t));
    o = static_cast<int32_t*>(container_o.ptr);
    if (rowindex) {
      arr32_t co(n, o);
      rowindex.extract_into(co);
      use_order = true;
    }
    if (make_groups) {
      groups.resize(n + 1);
      groups[0] = 0;
      gg.init(groups.data() + 1, 0);
    }
  }

  SortContext(size_t nrows, const RowIndex& rowindex, const Groupby& groupby,
              bool make_groups)
    : SortContext(nrows, rowindex, make_groups)
  {
    groups = arr32_t(groupby.size() + 1, groupby.offsets_r(), false);
    gg.init(nullptr, 0, groupby.size());
    if (!rowindex) {
      dt::parallel_for_static(n,
        [&](size_t i) {
          o[i] = static_cast<int32_t>(i);
        });
    }
  }


  SortContext(const SortContext&) = delete;
  SortContext(SortContext&&) = delete;


  void start_sort(const Column& col, bool desc) {
    column = col;
    descending = desc;
    if (desc) {
      _prepare_data_for_column<false>();
    } else {
      _prepare_data_for_column<true>();
    }
    if (n <= sort_insert_method_threshold) {
      if (use_order) {
        kinsert_sort();
      } else {
        vinsert_sort();
      }
    } else {
      if (groups) radix_psort<true>();
      else        radix_psort<false>();
    }
    use_order = true;  // if want to continue sort
  }


  void continue_sort(const Column& col, bool desc, bool make_groups) {
    column = col;
    nradixes = gg.size();
    descending = desc;
    xassert(nradixes > 0);
    xassert(o == container_o.ptr);
    if (desc) {
      _prepare_data_for_column<false>();
    } else {
      _prepare_data_for_column<true>();
    }
    if (is_string) strstart--;
    // Make sure that `xx` has enough storage capacity. Previous column may
    // have had smaller `elemsize` than this, in which case `xx` will need
    // to be expanded.
    next_elemsize = elemsize;
    allocate_xx();
    allocate_oo();

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


  RowIndex get_result_rowindex() {
    auto data = static_cast<int32_t*>(container_o.release());
    return RowIndex(arr32_t(n, data, true));
  }

  Groupby extract_groups() {
    size_t ng = gg.size();
    xassert(groups.size() > ng);
    groups.resize(ng + 1);
    return Groupby(ng, groups.to_memoryrange());
  }

  Groupby copy_groups() {
    size_t ng = gg.size();
    xassert(groups.size() > ng);
    size_t memsize = (ng + 1) * sizeof(int32_t);
    Buffer mr = Buffer::mem(memsize);
    std::memcpy(mr.xptr(), groups.data(), memsize);
    return Groupby(ng, std::move(mr));
  }

  std::pair<RowIndex, Groupby> get_result_groups() {
    size_t ng = gg.size();
    xassert(groups.size() > ng);
    groups.resize(ng + 1);
    return std::pair<RowIndex, Groupby>(get_result_rowindex(),
                                        Groupby(ng, groups.to_memoryrange()));
  }



  //============================================================================
  // Data preparation
  //============================================================================

  void allocate_x() {
    container_x.ensure_size(n * elemsize);
    x = rmem(container_x);
  }

  void allocate_xx() {
    container_xx.ensure_size(n * next_elemsize);
    xx = rmem(container_xx);
  }

  void allocate_oo() {
    container_oo.ensure_size(n * 4);
    next_o = static_cast<int*>(container_oo.ptr);
  }

  template <bool ASC>
  void _prepare_data_for_column() {
    is_string = false;
    // These will initialize `x`, `elemsize` and `nsigbits`, and also
    // `strdata`, `stroffs`, `strstart` for string columns
    SType stype = column.stype();
    switch (stype) {
      case SType::BOOL:    _initB<ASC>(); break;
      case SType::INT8:    _initI<ASC, int8_t,  uint8_t>(); break;
      case SType::INT16:   _initI<ASC, int16_t, uint16_t>(); break;
      case SType::INT32:   _initI<ASC, int32_t, uint32_t>(); break;
      case SType::INT64:   _initI<ASC, int64_t, uint64_t>(); break;
      case SType::FLOAT32: _initF<ASC, uint32_t>(); break;
      case SType::FLOAT64: _initF<ASC, uint64_t>(); break;
      case SType::STR32:
      case SType::STR64:   _initS<ASC>(); break;
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
   * (64 - x) >> 6  |   3   1   0   NA last,  DESC
   * (128 - x) >> 6 |   0   2   1   NA first, DESC
   */
  template <bool ASC>
  void _initB() {
    const uint8_t* xi = static_cast<const uint8_t*>(column.get_data_readonly());
    elemsize = 1;
    nsigbits = 2;
    allocate_x();
    uint8_t* xo = x.data<uint8_t>();

    if (use_order) {
      dt::parallel_for_static(n,
        [=](size_t j) {
          xo[j] = ASC? static_cast<uint8_t>(xi[o[j]] + 191) >> 6
                     : static_cast<uint8_t>(128 - xi[o[j]]) >> 6;
        });
    } else {
      dt::parallel_for_static(n,
        [=](size_t j) {
          // xi[j]+191 should be computed as uint8_t; by default C++ upcasts it
          // to int, which leads to wrong results after shift by 6.
          xo[j] = ASC? static_cast<uint8_t>(xi[j] + 191) >> 6
                     : static_cast<uint8_t>(128 - xi[j]) >> 6;
        });
    }
  }


  /**
   * For integer columns we subtract the min value, thus making the column
   * unsigned. Depending on the range of the values (max - min + 1), we cast
   * the data into an appropriate smaller type.
   */
  template <bool ASC, typename T, typename TU>
  void _initI() {
    xassert(sizeof(T) == sizeof(TU));
    int64_t min = column.stats()->min_int(nullptr);
    int64_t max = column.stats()->max_int(nullptr);
    nsigbits = sizeof(T) * 8;
    nsigbits -= dt::nlz(static_cast<TU>(max - min + 1));
    T edge = static_cast<T>(ASC? min : max);
    if (nsigbits > 32)      _initI_impl<ASC, T, TU, uint64_t>(edge);
    else if (nsigbits > 16) _initI_impl<ASC, T, TU, uint32_t>(edge);
    else if (nsigbits > 8)  _initI_impl<ASC, T, TU, uint16_t>(edge);
    else                    _initI_impl<ASC, T, TU, uint8_t >(edge);
  }

  template <bool ASC, typename T, typename TI, typename TO>
  void _initI_impl(T edge) {
    TI una = static_cast<TI>(GETNA<T>());
    TI uedge = static_cast<TI>(edge);
    const TI* xi = static_cast<const TI*>(column.get_data_readonly());
    elemsize = sizeof(TO);
    allocate_x();
    TO* xo = x.data<TO>();

    if (use_order) {
      dt::parallel_for_static(n,
        [&](size_t j) {
          TI t = xi[o[j]];
          xo[j] = t == una? 0 :
                  ASC? static_cast<TO>(t - uedge + 1)
                     : static_cast<TO>(uedge - t + 1);
        });
    } else {
      dt::parallel_for_static(n,
        [&](size_t j) {
          TI t = xi[j];
          xo[j] = t == una? 0 :
                  ASC? static_cast<TO>(t - uedge + 1)
                     : static_cast<TO>(uedge - t + 1);
        });
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
   * In order to put these values into ascending order, we'll do the following
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
  template <bool ASC, typename TO>
  void _initF() {
    const TO* xi = static_cast<const TO*>(column.get_data_readonly());
    elemsize = sizeof(TO);
    nsigbits = elemsize * 8;
    allocate_x();
    TO* xo = x.data<TO>();

    constexpr TO EXP
      = static_cast<TO>(sizeof(TO) == 8? 0x7FF0000000000000ULL : 0x7F800000);
    constexpr TO SIG
      = static_cast<TO>(sizeof(TO) == 8? 0x000FFFFFFFFFFFFFULL : 0x007FFFFF);
    constexpr TO SBT
      = static_cast<TO>(sizeof(TO) == 8? 0x8000000000000000ULL : 0x80000000);
    constexpr int SHIFT = sizeof(TO) * 8 - 1;

    if (use_order) {
      dt::parallel_for_static(n,
        [&](size_t j) {
          TO t = xi[o[j]];
          xo[j] = ((t & EXP) == EXP && (t & SIG) != 0) ? 0 :
                  ASC? t ^ (SBT | -(t>>SHIFT))
                     : t ^ (~SBT & ((t>>SHIFT) - 1));
        });
    } else {
      dt::parallel_for_static(n,
        [&](size_t j) {
          TO t = xi[j];
          xo[j] = ((t & EXP) == EXP && (t & SIG) != 0) ? 0 :
                  ASC? t ^ (SBT | -(t>>SHIFT))
                     : t ^ (~SBT & ((t>>SHIFT) - 1));
        });
    }
  }


  /**
   * For strings, we fill array `x` with the values of the first character in
   * each string. We also set up auxiliary variables `strstart` and `is_string`.
   *
   * More specifically, for each string item, if it is NA then we map it to 0;
   * if it is an empty string we map it to 1, otherwise we map it to `ch[i] + 2`
   * where `ch[i]` is the i-th character of the string. This doesn't overflow
   * because in UTF-8 the largest legal byte is 0xF7.
   */
  template <bool ASC>
  void _initS() {
    is_string = true;
    strstart = 0;
    nsigbits = 8;
    elemsize = 1;
    allocate_x();
    uint8_t* xo = x.data<uint8_t>();

    // `flong` is a flag that checks whether there is any string with len>1.
    std::atomic_flag flong = ATOMIC_FLAG_INIT;

    dt::parallel_region(
      dt::NThreads(nth),
      [&] {
        bool len_gt_1 = false;
        dt::nested_for_static(n, dt::ChunkSize(1024),
          [&](size_t j) {
            size_t k = use_order? static_cast<size_t>(o[j]) : j;
            CString value;
            bool isvalid = column.get_element(k, &value);
            if (isvalid) {
              if (value.size) {
                xo[j] = ASC? static_cast<uint8_t>(*value.ch) + 2
                           : 0xFE - static_cast<uint8_t>(*value.ch);
                len_gt_1 |= (value.size > 1);
              } else {
                xo[j] = ASC? 1 : 0xFF;  // empty string
              }
            } else {
              xo[j] = 0;    // NA string
            }
          });
        if (len_gt_1) flong.test_and_set();
      });
    next_elemsize = flong.test_and_set();
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
    size_t nch = nth * sort_thread_multiplier;
    chunklen = std::max((n - 1) / nch + 1,
                        sort_max_chunk_length);
    nchunks = (n - 1)/chunklen + 1;

    uint8_t nradixbits = nsigbits < sort_max_radix_bits
                         ? nsigbits : sort_over_radix_bits;
    shift = nsigbits - nradixbits;
    nradixes = 1 << nradixbits;

    // The remaining number of sig.bits is `shift`. Thus, this value will
    // determine the `next_elemsize`.
    next_elemsize = is_string? 1 :
                    shift > 32? 8 :
                    shift > 16? 4 :
                    shift > 0? 2 : 0;
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
    T* tx = x.data<T>();
    dt::parallel_for_static(nchunks, dt::ChunkSize(1),
      [&](size_t i) {
        size_t* cnts = histogram + (nradixes * i);
        size_t j0 = i * chunklen;
        size_t j1 = std::min(j0 + chunklen, n);
        for (size_t j = j0; j < j1; ++j) {
          cnts[tx[j] >> shift]++;
        }
      });
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
   * Perform the main radix shuffle, filling in arrays `next_o` and `xx`.
   * The array `next_o` will contain the original row numbers of the values in
   * `x` such that `x[next_o]` is sorted with respect to the most significant
   * bits. The `xx` array will contain the sorted elements of `x`, with MSB
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
    if (!xx && next_elemsize) allocate_xx();
    if (!next_o) allocate_oo();
    if (is_string) {
      if (xx) {
        if (descending) _reorder_str<false>();
        else            _reorder_str<true>();
      } else {
        _reorder_impl<uint8_t, char, false>();
      }
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
    swap(x, xx);
    std::swap(o, next_o);
    std::swap(elemsize, next_elemsize);
    use_order = true;
  }

  template<typename TI, typename TO, bool OUT>
  void _reorder_impl() {
    TI* xi = x.data<TI>();
    TO* xo = nullptr;
    TI mask = 0;
    if (OUT) {
      xo = xx.data<TO>();
      mask = static_cast<TI>((1ULL << shift) - 1);
    }
    dt::parallel_for_static(nchunks, dt::ChunkSize(1),
      [&](size_t i) {
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
      });
    xassert(histogram[nchunks * nradixes - 1] == n);
  }

  template <bool ASC>
  void _reorder_str() {
    uint8_t* xi = x.data<uint8_t>();
    uint8_t* xo = xx.data<uint8_t>();
    const int64_t sstart = static_cast<int64_t>(strstart) + 1;
    std::atomic_flag flong = ATOMIC_FLAG_INIT;

    dt::parallel_region(dt::NThreads(nth),
      [&] {
        bool tlong = false;
        dt::nested_for_static(nchunks, dt::ChunkSize(1),
          [&](size_t i) {
            size_t j0 = i * chunklen;
            size_t j1 = std::min(j0 + chunklen, n);
            size_t* tcounts = histogram + (nradixes * i);
            for (size_t j = j0; j < j1; ++j) {
              size_t k = tcounts[xi[j]]++;
              xassert(k < n);
              size_t w = use_order? static_cast<size_t>(o[j]) : j;
              CString value;
              bool isvalid = column.get_element(w, &value);
              if (isvalid) {
                if (value.size > sstart) {
                  xo[k] = ASC? static_cast<uint8_t>(value.ch[sstart] + 2)
                             : static_cast<uint8_t>(0xFE - value.ch[sstart]);
                  tlong = true;
                } else {
                  xo[k] = ASC? 1 : 0xFF;  // string is shorter than sstart
                }
              } else {
                xo[k] = 0;
              }
              next_o[k] = static_cast<int32_t>(w);
            }
          });
        if (tlong) flong.test_and_set();
      });
    next_elemsize = flong.test_and_set();
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
  template <bool make_groups>
  void radix_psort() {
    int32_t* ores = o;
    determine_sorting_parameters();
    build_histogram();
    reorder_data();

    if (elemsize) {
      // If after reordering there are still unsorted elements in `x`, then
      // sort them recursively.
      uint8_t _nsigbits = nsigbits;
      nsigbits = is_string? 8 : shift;
      dt::array<radix_range> rrmap(nradixes);
      radix_range* rrmap_ptr = rrmap.data();
      _fill_rrmap_from_histogram(rrmap_ptr);
      _radix_recurse<make_groups>(rrmap_ptr);
      nsigbits = _nsigbits;
    }
    else if (make_groups) {
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
    size_t ng = gg.size();
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
    xassert(x && xx && o && next_o);
    // Save some of the variables in SortContext that we will be modifying
    // in order to perform the recursion.
    size_t   _n        = n;
    rmem     _x        { x };
    rmem     _xx       { xx };
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
    size_t rrlarge = sort_insert_method_threshold;  // for now
    xassert(GROUPED > rrlarge);

    strstart = _strstart + 1;

    for (size_t rri = 0; rri < _nradixes; ++rri) {
      size_t sz = rrmap[rri].size;
      if (sz > rrlarge) {
        size_t off = rrmap[rri].offset;
        elemsize = _elemsize;
        n = sz;
        x = rmem(_x, off * elemsize, n * elemsize);
        xx = rmem(_xx, off * elemsize, n * elemsize);
        o = _o + off;
        next_o = _next_o + off;
        if (make_groups) {
          gg.init(ggdata0 + off, ggoff0 + static_cast<int32_t>(off));
          radix_psort<true>();
          rrmap[rri].size = gg.size() | GROUPED;
        } else {
          radix_psort<false>();
        }
      } else {
        nsmallgroups += (sz > 1);
        if (sz > size0) size0 = sz;
      }
    }

    n = _n;
    x = _x;
    xx = _xx;
    o = _o;
    next_o = _next_o;
    elemsize = _elemsize;
    nradixes = _nradixes;
    strstart = _strstart;
    gg.init(ggdata0, ggoff0);

    // Finally iterate over all remaining radix ranges, in-parallel, and
    // sort each of them independently using a simpler insertion sort
    // method.
    int32_t* tmp = nullptr;
    bool own_tmp = false;
    if (size0) {
      // size_t size_all = size0 * nthreads * sizeof(int32_t);
      // if ((size_t)_next_elemsize * _n <= size_all) {
      //   tmp = (int32_t*)_x;
      // } else {
      own_tmp = true;
      tmp = new int32_t[size0 * nth];
      TRACK(tmp, sizeof(tmp), "sort.tmp");
      // }
    }

    dt::parallel_region(dt::NThreads(nth),
      [&] {
        size_t tnum = dt::this_thread_index();
        int32_t* oo = tmp + tnum * size0;
        GroupGatherer tgg;

        dt::nested_for_static(
          /* n_iterations */ _nradixes,
          [&](size_t i) {
            size_t zn  = rrmap[i].size;
            size_t off = rrmap[i].offset;
            if (zn > rrlarge) {
              rrmap[i].size = zn & ~GROUPED;
            } else if (zn > 1) {
              int32_t  tn = static_cast<int32_t>(zn);
              rmem     tx { _x, off * elemsize, zn * elemsize };
              int32_t* to = _o + off;
              if (make_groups) {
                tgg.init(ggdata0 + off, static_cast<int32_t>(off) + ggoff0);
              }
              if (is_string) {
                insert_sort_keys_str(column, _strstart + 1, to, oo, tn, tgg, descending);
              } else {
                switch (elemsize) {
                  case 1: insert_sort_keys<>(tx.data<uint8_t>(), to, oo, tn, tgg); break;
                  case 2: insert_sort_keys<>(tx.data<uint16_t>(), to, oo, tn, tgg); break;
                  case 4: insert_sort_keys<>(tx.data<uint32_t>(), to, oo, tn, tgg); break;
                  case 8: insert_sort_keys<>(tx.data<uint64_t>(), to, oo, tn, tgg); break;
                }
              }
              if (make_groups) {
                rrmap[i].size = static_cast<size_t>(tgg.size());
              }
            } else if (zn == 1 && make_groups) {
              ggdata0[off] = static_cast<int32_t>(off) + ggoff0 + 1;
              rrmap[i].size = 1;
            }
          });  // dt::nested_for_static
      });  // dt::parallel_region

    // Consolidate groups into a single contiguous chunk
    if (make_groups) {
      gg.from_chunks(rrmap, _nradixes);
    }

    if (own_tmp) {
      delete[] tmp;
      UNTRACK(tmp);
    }
  }



  //============================================================================
  // Insert sort
  //============================================================================

  void kinsert_sort() {
    arr32_t tmparr(n);
    int32_t* tmp = tmparr.data();
    int32_t nn = static_cast<int32_t>(n);
    if (is_string) {
      insert_sort_keys_str(column, 0, o, tmp, nn, gg, descending);
    } else {
      switch (elemsize) {
        case 1: _insert_sort_keys<uint8_t >(tmp); break;
        case 2: _insert_sort_keys<uint16_t>(tmp); break;
        case 4: _insert_sort_keys<uint32_t>(tmp); break;
        case 8: _insert_sort_keys<uint64_t>(tmp); break;
      }
    }
  }

  void vinsert_sort() {
    if (is_string) {
      int32_t nn = static_cast<int32_t>(n);
      insert_sort_values_str(column, 0, o, nn, gg, descending);
    } else {
      switch (elemsize) {
        case 1: _insert_sort_values<uint8_t >(); break;
        case 2: _insert_sort_values<uint16_t>(); break;
        case 4: _insert_sort_values<uint32_t>(); break;
        case 8: _insert_sort_values<uint64_t>(); break;
      }
    }
  }

  template <typename T> void _insert_sort_keys(int32_t* tmp) {
    T* xt = x.data<T>();
    int32_t nn = static_cast<int32_t>(n);
    insert_sort_keys(xt, o, tmp, nn, gg);
  }

  template <typename T> void _insert_sort_values() {
    T* xt = x.data<T>();
    int32_t nn = static_cast<int32_t>(n);
    insert_sort_values(xt, o, nn, gg);
  }

};



//==============================================================================
// Main sorting routines
//==============================================================================

RiGb group(const std::vector<Column>& columns,
           const std::vector<SortFlag>& flags)
{
  RiGb result;
  size_t n = columns.size();
  xassert(n > 0);
  xassert(n == flags.size());

  const Column& col0 = columns[0];
  col0.stats();  // instantiate Stats object; TODO: remove this

  size_t nrows = col0.nrows();
  #if DTDEBUG
    for (const Column& col : columns) xassert(col.nrows() == nrows);
  #endif

  // For a 0-row Frame we return a rowindex of size 0, and the
  // Groupby containing zero groups.
  if (nrows == 0) {
    result.second = Groupby::zero_groups();
    return result;
  }
  if (nrows == 1) {
    arr32_t indices(nrows);
    indices[0] = 0;
    result.first = RowIndex(std::move(indices), true);
    result.second = Groupby::single_group(nrows);
    return result;
  }

  // TODO: avoid materialization
  for (const Column& col : columns) {
    const_cast<Column&>(col).materialize();
  }

  bool do_groups = n > 1 || !(flags[0] & SortFlag::SORT_ONLY);
  SortContext sc(nrows, RowIndex(), do_groups);
  sc.start_sort(col0, bool(flags[0] & SortFlag::DESCENDING));
  for (size_t j = 1; j < n; ++j) {
    bool j_sort_only = (flags[j] & SortFlag::SORT_ONLY);
    bool j_descending = (flags[j] & SortFlag::DESCENDING);
    if (j_sort_only && !(flags[j - 1] & SortFlag::SORT_ONLY)) {
      result.second = sc.copy_groups();
    }
    if (j == n - 1 && j_sort_only) {
      do_groups = false;
    }
    const Column& colj = columns[j];
    colj.stats();  // TODO: remove this
    sc.continue_sort(colj, j_descending, do_groups);
  }
  result.first = sc.get_result_rowindex();
  if (!(flags[0] & SortFlag::SORT_ONLY) && !result.second) {
    result.second = sc.extract_groups();
  }
  return result;
}



void dt::ColumnImpl::sort_grouped(const Groupby& grps, Column& out) {
  (void) out.stats();
  SortContext sc(nrows(), RowIndex(), grps, /* make_groups = */ false);
  sc.continue_sort(out, /* desc = */ false, /* make_groups = */ false);
  out.apply_rowindex(sc.get_result_rowindex());
}


template <typename T>
void dt::ArrayView_ColumnImpl<T>::sort_grouped(
    const Groupby& grps, Column& out)
{
  (void) out.stats();
  SortContext sc(nrows(), rowindex_container, grps, /*make_groups=*/ false);
  sc.continue_sort(arg, /*desc=*/ false, /*make_groups=*/ false);
  set_rowindex(sc.get_result_rowindex());
}

template class dt::ArrayView_ColumnImpl<int32_t>;
template class dt::ArrayView_ColumnImpl<int64_t>;



//------------------------------------------------------------------------------
// py::Frame.sort
//------------------------------------------------------------------------------

static py::PKArgs args_sort(
  0, 0, 0, true, false, {}, "sort",

R"(sort(self, *cols)
--

Sort frame by the specified column(s).

Parameters
----------
cols: List[str | int]
    Names or indices of the columns to sort by. If no columns are
    given, the Frame will be sorted on all columns.

Returns
-------
New Frame sorted by the provided column(s). The current frame
remains unmodified.
)");

py::oobj py::Frame::sort(const PKArgs& args) {
  dt::expr::EvalContext ctx(dt);

  if (args.num_vararg_args() == 0) {
    py::otuple all_cols(dt->ncols());
    for (size_t i = 0; i < dt->ncols(); ++i) {
      all_cols.set(i, py::oint(i));
    }
    ctx.add_sortby(py::osort(all_cols));
  }
  else {
    std::vector<py::robj> cols;
    for (py::robj arg : args.varargs()) {
      if (arg.is_list_or_tuple()) {
        auto arg_as_list = arg.to_pylist();
        for (size_t i = 0; i < arg_as_list.size(); ++i) {
          cols.push_back(arg_as_list[i]);
        }
      } else {
        cols.push_back(arg);
      }
    }

    py::otuple sort_cols(cols.size());
    for (size_t i = 0; i < cols.size(); ++i) {
      sort_cols.set(i, cols[i]);
    }
    ctx.add_sortby(py::osort(sort_cols));
  }

  ctx.add_i(py::None());
  ctx.add_j(py::None());
  return ctx.evaluate();
}



void py::Frame::_init_sort(XTypeMaker& xt) {
  xt.add(METHOD(&Frame::sort, args_sort));
}
