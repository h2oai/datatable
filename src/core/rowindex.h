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
#include <cstdint>
#include "utils/array.h"

class Column;
class RowIndexImpl;


enum class RowIndexType : uint8_t {
  UNKNOWN = 0,
  ARR32 = 1,
  ARR64 = 2,
  SLICE = 3,
};



//==============================================================================
// Main RowIndex class
//==============================================================================

class RowIndex {
  private:
    RowIndexImpl* impl;  // Shared reference semantics

  public:
    static constexpr int32_t NA_ARR32 = INT32_MIN;
    static constexpr int64_t NA_ARR64 = INT64_MIN;
    static constexpr size_t MAX = size_t(-1) >> 1;
    static_assert(int32_t(size_t(NA_ARR32)) == NA_ARR32, "Bad NA_ARR32");
    static_assert(int64_t(size_t(NA_ARR64)) == NA_ARR64, "Bad NA_ARR64");

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
     * Create RowIndex from either a boolean or an integer column.
     */
    RowIndex(const Column& col);


    // Create a new rowindex by concatenating the supplied list of rowindices.
    static RowIndex concat(const std::vector<RowIndex>&);


    bool operator==(const RowIndex& other) { return impl == other.impl; }
    bool operator!=(const RowIndex& other) { return impl != other.impl; }
    operator bool() const { return impl != nullptr; }

    RowIndexType type() const noexcept;
    bool isabsent() const;
    bool isslice() const;
    bool isarr32() const;
    bool isarr64() const;
    bool is_all_missing() const;

    size_t size() const;
    size_t max() const;
    const int32_t* indices32() const noexcept;
    const int64_t* indices64() const noexcept;
    size_t slice_start() const noexcept;
    size_t slice_step() const noexcept;

    bool get_element(size_t i, size_t* out) const;

    void extract_into(arr32_t&) const;
    void extract_into(arr64_t&) const;

    /**
     * Convert the RowIndex into an array `int8_t[nrows]`, where each entry
     * is either 1 or 0 depending whether that element is selected by the
     * current RowIndex or not.
     */
    Buffer as_boolean_mask(size_t nrows) const;

    /**
     * Convert the RowIndex into an array `int32_t[nrows]`, where entries not
     * selected by this RowIndex are -1, and the selected entries are
     * consecutive integers 0, 1, ..., size()-1.
     */
    Buffer as_integer_mask(size_t nrows) const;

    /**
     * Return a RowIndex which is the negation of the current, when applied
     * to an array of `nrows` elements. That is, the returned RowIndex
     * contains all elements in the `range(nrows)` which are *not* selected
     * by the current RowIndex.
     *
     * The parameter `nrows` must be greater than the largest entry in the
     * current RowIndex.
     */
    RowIndex negate(size_t nrows) const;

    /**
     * Return the RowIndex which is the result of applying RowIndex `ab` to
     * RowIndex `bc`. More specifically, suppose there are 2 frames A and B,
     * with A being a subset of B, and that RowIndex `ab` describes which
     * rows in B are selected into A. Furthermore, suppose B itself is a
     * subframe of C, and RowIndex `bc` describes which rows from C are
     * selected into B. Then `ab * bc` will produce a new RowIndex
     * object describing how the rows of A can be obtained from C.
     *
     * Note that the product is not commutative: `ab * bc` != `bc * ab`.
     */
    friend RowIndex operator *(const RowIndex& ab, const RowIndex& bc);

    /**
     * Template function that allows looping through the RowIndex. This
     * method will run the equivalent of
     *
     *     for i in range(istart, iend, istep):
     *         j = self[i]
     *         f(i, j)
     *
     * Function `f` is expected to have a signature `void(size_t i, size_t j)`.
     */
    template<typename F>
    void iterate(size_t istart, size_t iend, size_t istep, F f) const;

    size_t memory_footprint() const noexcept;
    void verify_integrity() const;

  private:
    RowIndex(RowIndexImpl* rii);
};




//==============================================================================

template<typename F>
void RowIndex::iterate(size_t i0, size_t i1, size_t di, F f) const {
  switch (type()) {
    case RowIndexType::UNKNOWN: {
      for (size_t i = i0; i < i1; i += di) {
        f(i, i, true);
      }
      break;
    }
    case RowIndexType::ARR32: {
      const int32_t* ridata = indices32();
      for (size_t i = i0; i < i1; i += di) {
        int32_t x = ridata[i];
        f(i, static_cast<size_t>(x), x != RowIndex::NA_ARR32);
      }
      break;
    }
    case RowIndexType::ARR64: {
      const int64_t* ridata = indices64();
      for (size_t i = i0; i < i1; i += di) {
        int64_t x = ridata[i];
        f(i, static_cast<size_t>(x), x != RowIndex::NA_ARR64);
      }
      break;
    }
    case RowIndexType::SLICE: {
      // Careful with negative slice steps (in which case j goes down,
      // and therefore we cannot iterate until `j < jend`).
      size_t jstep = slice_step() * di;
      size_t j = slice_start() + i0 * slice_step();
      for (size_t i = i0; i < i1; i += di) {
        f(i, j, true);
        j += jstep;
      }
      break;
    }
  }
}


bool check_slice_triple(size_t start, size_t cnt, size_t step, size_t max);


#endif
