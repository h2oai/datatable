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
#ifndef dt_ROWINDEX_h
#define dt_ROWINDEX_h
#include "utils/array.h"

class Column;
class BoolColumn;
class RowIndexImpl;


enum class RowIndexType : uint8_t {
  UNKNOWN = 0,
  ARR32 = 1,
  ARR64 = 2,
  SLICE = 3,
};

using filterfn32 = int (size_t row0, size_t row1, int32_t* ind, size_t* nouts);
using filterfn64 = int (size_t row0, size_t row1, int64_t* ind, size_t* nouts);



//==============================================================================
// Main RowIndex class
//==============================================================================

class RowIndex {
  private:
    RowIndexImpl* impl;  // Shared reference semantics

  public:
    static constexpr size_t NA = size_t(-1);
    static constexpr size_t MAX = size_t(-1) >> 1;

    RowIndex();
    RowIndex(const RowIndex&);
    RowIndex(RowIndex&&);
    RowIndex& operator=(const RowIndex&);
    ~RowIndex();

    /**
     * Construct a RowIndex object from an array of int32/int64 indices.
     * Optional `sorted` flag tells the constructor whether the arrays are
     * sorted or not.
     */
    RowIndex(arr32_t&& arr, bool sorted = false);
    RowIndex(arr64_t&& arr, bool sorted = false);

    /**
     * Construct a "slice" RowIndex from triple `(start, count, step)`.
     *
     * Note that we depart from Python's standard of using `(start, end, step)`
     * to denote a slice -- having a `count` gives several advantages:
     *   - computing the "end" is easy and unambiguous: `start + count * step`;
     *     whereas computing "count" from `end` is harder.
     *   - with explicit `count` the `step` may potentially be 0.
     *   - there is no difference in handling positive/negative steps.
     */
    RowIndex(size_t start, size_t count, size_t step);

    /**
     * Construct an "array" `RowIndex` object from a series of triples
     * `(start, count, step)`. The triples are given as 3 separate arrays of
     * starts, of counts and of steps.
     *
     * This will create either an RowIndexType::ARR32 or RowIndexType::ARR64
     * object, depending on which one is sufficient to hold all the indices.
     */
    RowIndex(const arr64_t& starts, const arr64_t& counts, const arr64_t& steps);

    /**
     * Construct a RowIndex object using an external filter function. The
     * provided filter function is expected to take a range of rows `row0:row1`
     * and an output buffer, and writes the indices of the selected rows into
     * that buffer. This function then handles assembling that output into a
     * final RowIndex structure, as well as distributing the work load among
     * multiple threads.
     *
     * @param f
     *     Pointer to the filter function with the signature `(row0, row1, out,
     *     &nouts) -> int`. The filter function has to determine which rows in
     *     the range `row0:row1` are to be included, and write their indices
     *     into the array `out`. It should also store in the variable `nouts`
     *     the number of rows selected.
     *
     * @param n
     *     Number of rows in the datatable that is being filtered.
     *
     * @param sorted
     *     When True indicates that the filter function is guaranteed to produce
     *     row index in sorted order.
     */
    RowIndex(filterfn32* f, size_t n, bool sorted);
    RowIndex(filterfn64* f, size_t n, bool sorted);

    /**
     * Create RowIndex from either a boolean or an integer column.
     */
    RowIndex(const Column* col);


    bool operator==(const RowIndex& other) { return impl == other.impl; }
    bool operator!=(const RowIndex& other) { return impl != other.impl; }
    operator bool() const { return impl != nullptr; }

    RowIndexType type() const;
    bool isabsent() const;
    bool isslice() const;
    bool isarr32() const;
    bool isarr64() const;
    bool isarray() const;
    const void* ptr() const;

    size_t size() const;
    size_t min() const;
    size_t max() const;
    size_t nth(size_t i) const;
    const int32_t* indices32() const;
    const int64_t* indices64() const;
    size_t slice_start() const;
    size_t slice_step() const;

    void extract_into(arr32_t&) const;
    RowIndex inverse(size_t nrows) const;

    /**
     * Return the RowIndex which is the result of applying RowIndex `ab` to
     * RowIndex `bc`. More specifically, suppose there are 2 frames A and B,
     * with A being a subset of B, and that `this` RowIndex describes which
     * rows in B are selected into A. Furthermore, suppose B itself is a
     * subframe of C, and RowIndex `other` describes which rows from C are
     * selected into B. Then `AB * BC` will return a new RowIndex
     * object describing how the rows of A can be obtained from C.
     *
     * Note that the product is not commutative: `ab * bc` != `bc * ab`.
     */
    friend RowIndex operator *(const RowIndex& ab, const RowIndex& bc);

    void clear();

    /**
     * Reduce the size of the RowIndex to `nrows` elements. The second parameter
     * indicates the number of columns in the DataTable to which the RowIndex
     * belongs. Since each Column in the DataTable carries a copy of the
     * RowIndex. Knowing the number of columns allows us to know when the
     * RowIndex can be safely modified in-place, and when it cannot.
     */
    void shrink(size_t nrows, size_t ncols);

    size_t memory_footprint() const;

    /**
     * Template function that facilitates looping through a RowIndex.
     */
    template<typename F> void strided_loop2(
        size_t istart, size_t iend, size_t istep, F f) const;

    void verify_integrity() const;

  private:
    RowIndex(RowIndexImpl* rii);
};




//==============================================================================

template<typename F>
void RowIndex::strided_loop2(
    size_t istart, size_t iend, size_t istep, F f) const
{
  switch (type()) {
    case RowIndexType::UNKNOWN: {
      for (size_t i = istart; i < iend; i += istep) {
        f(i, i);
      }
      break;
    }
    case RowIndexType::ARR32: {
      const int32_t* ridata = indices32();
      for (size_t i = istart; i < iend; i += istep) {
        f(i, static_cast<size_t>(ridata[i]));
      }
      break;
    }
    case RowIndexType::ARR64: {
      const int64_t* ridata = indices64();
      for (size_t i = istart; i < iend; i += istep) {
        f(i, static_cast<size_t>(ridata[i]));
      }
      break;
    }
    case RowIndexType::SLICE: {
      // Careful with negative slice steps (in which case j goes down,
      // and therefore we cannot iterate until `j < jend`).
      size_t jstep = slice_step() * istep;
      size_t j = slice_start() + istart * slice_step();
      for (size_t i = istart; i < iend; i += istep) {
        f(i, j);
        j += jstep;
      }
      break;
    }
  }
}


bool check_slice_triple(size_t start, size_t cnt, size_t step, size_t max);


#endif
