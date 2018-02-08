//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#ifndef dt_ROWINDEX_H
#define dt_ROWINDEX_H
#include <string>   // std::string
#include "utils/array.h"

class Column;
class BoolColumn;
class RowIndex;
class IntegrityCheckContext;


//==============================================================================

enum RowIndexType {
  RI_UNKNOWN = 0,
  RI_ARR32 = 1,
  RI_ARR64 = 2,
  RI_SLICE = 3,
};

typedef int (filterfn32)(int64_t, int64_t, int32_t*, int32_t*);
typedef int (filterfn64)(int64_t, int64_t, int64_t*, int32_t*);



//==============================================================================
// Base RowIndexImpl class
//==============================================================================

class RowIndexImpl {
  public:
    RowIndexType type;
    int32_t refcount;
    int64_t length;
    int64_t min;
    int64_t max;

    RowIndexImpl()
      : type(RowIndexType::RI_UNKNOWN),
        refcount(1),
        length(0), min(0), max(0) {}
    void acquire() { refcount++; }
    void release() { if (!--refcount) delete this; }

  protected:
    virtual ~RowIndexImpl() {}
};



//==============================================================================
// "Array" RowIndexImpl class
//==============================================================================

class ArrayRowIndexImpl : public RowIndexImpl {
  private:
    dt::array<int32_t> ind32;
    dt::array<int64_t> ind64;

  public:
    ArrayRowIndexImpl(dt::array<int32_t>&& indices, bool sorted);
    ArrayRowIndexImpl(dt::array<int64_t>&& indices, bool sorted);
    ArrayRowIndexImpl(const dt::array<int64_t>& starts,
                      const dt::array<int64_t>& counts,
                      const dt::array<int64_t>& steps);
    ArrayRowIndexImpl(filterfn32* f, int64_t n, bool sorted);
    ArrayRowIndexImpl(filterfn64* f, int64_t n, bool sorted);
    ArrayRowIndexImpl(Column*);

    const int32_t* indices32() const { return ind32.data(); }
    const int64_t* indices64() const { return ind64.data(); }

  private:
    // Helper function that computes and sets proper `min` / `max` fields for
    // this RowIndex. The `sorted` flag is a hint whether the indices are
    // sorted (if they are, computing min/max is much simpler).
    template <typename T> void set_min_max(const dt::array<T>&, bool sorted);

    // Helpers for `ArrayRowIndexImpl(Column*)`
    void init_from_boolean_column(BoolColumn* col);
    void init_from_integer_column(Column* col);
};



//==============================================================================
// "Slice" RowIndexImpl class
//==============================================================================

class SliceRowIndexImpl : public RowIndexImpl {
  protected:
    int64_t start;
    int64_t step;

  public:
    SliceRowIndexImpl(int64_t start, int64_t count, int64_t step);
    static void check_triple(int64_t start, int64_t count, int64_t step);

  protected:
    friend RowIndex;
};



//==============================================================================
// Main RowIndex class
//==============================================================================

class RowIndex {
  private:
    RowIndexImpl* impl;

  public:
    RowIndex() : impl(nullptr) {}
    RowIndex(const RowIndex&);
    RowIndex& operator=(const RowIndex&);
    ~RowIndex();

    /**
     * Construct a RowIndex object from an array of int32/int64 indices.
     * Optional `sorted` flag tells the constructor whether the arrays are
     * sorted or not.
     */
    static RowIndex from_array32(dt::array<int32_t>&& arr, bool sorted = false);
    static RowIndex from_array64(dt::array<int64_t>&& arr, bool sorted = false);

    static RowIndex from_slice(int64_t start, int64_t count, int64_t step);
    static RowIndex from_slices(const dt::array<int64_t>& starts,
                                const dt::array<int64_t>& counts,
                                const dt::array<int64_t>& steps);

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
    static RowIndex from_filterfn32(filterfn32* f, int64_t n, bool sorted);
    static RowIndex from_filterfn64(filterfn64* f, int64_t n, bool sorted);

    static RowIndex from_column(Column* col);


    bool operator==(const RowIndex& other) { return impl == other.impl; }
    operator bool() const { return impl != nullptr; }

    bool isabsent() const { return impl == nullptr; }
    bool isslice() const { return impl && impl->type == RI_SLICE; }
    bool isarr32() const { return impl && impl->type == RI_ARR32; }
    bool isarr64() const { return impl && impl->type == RI_ARR64; }
    bool isarray() const { return isarr32() || isarr64(); }

    int64_t length() const { return impl? impl->length : 0; }
    size_t zlength() const { return static_cast<size_t>(length()); }
    int64_t min() const { return impl? impl->min : 0; }
    int64_t max() const { return impl? impl->max : 0; }
    const int32_t* indices32() const { return impl_asarray()->indices32(); }
    const int64_t* indices64() const { return impl_asarray()->indices64(); }
    int64_t slice_start() const { return impl_asslice()->start; }
    int64_t slice_step() const { return impl_asslice()->step; }

    dt::array<int32_t> extract_as_array32() const;

    RowIndex merged_with(const RowIndex&) const;

    void clear();
    size_t memory_footprint() const { return 0; } // TODO

    /**
     * Template function that facilitates looping through a RowIndex.
     */
    template<typename F> void strided_loop(
        int64_t istart, int64_t iend, int64_t istep, F f) const;

    bool verify_integrity(IntegrityCheckContext&,
                          const std::string& = "RowIndex") const { return true; }

  private:
    RowIndex(RowIndexImpl* rii) : impl(rii) {}
    ArrayRowIndexImpl* impl_asarray() const {
      return static_cast<ArrayRowIndexImpl*>(impl);
    }
    SliceRowIndexImpl* impl_asslice() const {
      return static_cast<SliceRowIndexImpl*>(impl);
    }
};


//==============================================================================

template<typename F>
void RowIndex::strided_loop(
    int64_t istart, int64_t iend, int64_t istep, F f) const
{
  switch (impl? impl->type : RowIndexType::RI_UNKNOWN) {
    case RI_UNKNOWN: {
      for (int64_t i = istart; i < iend; i += istep) {
        f(i);
      }
      break;
    }
    case RI_ARR32: {
      const int32_t* ridata = indices32();
      for (int64_t i = istart; i < iend; i += istep) {
        f(static_cast<int64_t>(ridata[i]));
      }
      break;
    }
    case RI_ARR64: {
      const int64_t* ridata = indices64();
      for (int64_t i = istart; i < iend; i += istep) {
        f(ridata[i]);
      }
      break;
    }
    case RI_SLICE: {
      // Careful with negative slice steps (in which case j goes down,
      // and therefore we cannot iterate until `j < jend`).
      int64_t jstep = slice_step() * istep;
      int64_t j = slice_start() + istart * slice_step();
      for (int64_t i = istart; i < iend; i += istep) {
        f(j);
        j += jstep;
      }
      break;
    }
  }
}


#endif
