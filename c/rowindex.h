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

class Column;
class RowIndex;
class IntegrityCheckContext;



//==============================================================================

enum RowIndexType {
  RI_UNKNOWN = 0,
  RI_ARR32 = 1,
  RI_ARR64 = 2,
  RI_SLICE = 3,
};

typedef int (rowindex_filterfn32)(int64_t, int64_t, int32_t*, int32_t*);
typedef int (rowindex_filterfn64)(int64_t, int64_t, int64_t*, int32_t*);



//==============================================================================

class RowIndexImpl {
  private:
    int32_t refcount;
  public:
    RowIndexType type;
    int64_t length;
    int64_t min;
    int64_t max;

  public:
    RowIndexImpl();
    void acquire() { refcount++; }
    void release();

  protected:
    virtual ~RowIndexImpl() {}
};


class ArrayRowIndexImpl : public RowIndexImpl {
  public:
    union {
      int32_t* ind32;
      int64_t* ind64;
    };

    ArrayRowIndexImpl(int32_t* indices, int64_t n, bool sorted);
    ArrayRowIndexImpl(int64_t* indices, int64_t n, bool sorted);
    ArrayRowIndexImpl(int64_t* starts, int64_t* counts, int64_t* steps,
                      int64_t n);

  protected:
    virtual ~ArrayRowIndexImpl();
};


class SliceRowIndexImpl : public RowIndexImpl {
  public:
    int64_t start;
    int64_t step;

    SliceRowIndexImpl(int64_t start, int64_t count, int64_t step);
    static void check_triple(int64_t start, int64_t count, int64_t step);

  protected:
    virtual ~SliceRowIndexImpl() {}
};



//==============================================================================

class RowIndeZ {
  private:
    RowIndexImpl* impl;

  public:
    RowIndeZ() : impl(nullptr) {}
    RowIndeZ(const RowIndeZ&);
    RowIndeZ& operator=(const RowIndeZ&);
    RowIndeZ(RowIndex* old);  // TODO: remove
    static RowIndeZ from_array32(int32_t* arr, int64_t n, bool issorted);
    static RowIndeZ from_array64(int64_t* arr, int64_t n, bool issorted);
    static RowIndeZ from_slice(int64_t start, int64_t count, int64_t step);
    static RowIndeZ from_slices(int64_t* starts, int64_t* counts,
                                int64_t* steps, int64_t n);

    ~RowIndeZ();

    inline bool isabsent() const { return impl == nullptr; }
    inline bool isslice() const { return impl && impl->type == RI_SLICE; }
    inline bool isarr32() const { return impl && impl->type == RI_ARR32; }
    inline bool isarr64() const { return impl && impl->type == RI_ARR64; }
    inline bool isarray() const { return isarr32() || isarr64(); }

    inline int64_t length() const { return impl? impl->length : 0; }
    inline int64_t min() const { return impl? impl->min : 0; }
    inline int64_t max() const { return impl? impl->max : 0; }
    inline int32_t* indices32() const { return impl_asarray()->ind32; }
    inline int64_t* indices64() const { return impl_asarray()->ind64; }
    inline int64_t slice_start() const { return impl_asslice()->start; }
    inline int64_t slice_step() const { return impl_asslice()->step; }

    int32_t* extract_as_array32() const;

    /**
     * Template function that facilitates looping through a RowIndex.
     */
    template<typename F>
    inline void strided_loop(
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
          int32_t* ridata = indices32();
          for (int64_t i = istart; i < iend; i += istep) {
            f(static_cast<int64_t>(ridata[i]));
          }
          break;
        }
        case RI_ARR64: {
          int64_t* ridata = indices64();
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

  private:
    RowIndeZ(RowIndexImpl* rii) : impl(rii) {}
    inline ArrayRowIndexImpl* impl_asarray() const {
      return dynamic_cast<ArrayRowIndexImpl*>(impl);
    }
    inline SliceRowIndexImpl* impl_asslice() const {
      return dynamic_cast<SliceRowIndexImpl*>(impl);
    }
};


//==============================================================================





//==============================================================================

/**
 * type
 *     The type of the RowIndex: either array or a slice. Two kinds of arrays
 *     are supported: one with 32-bit indices and having no more than 2**31-1
 *     elements, and the other with 64-bit indices and having up to 2**63-1
 *     items.
 *
 * length
 *     Total number of elements in the RowIndex (corresponds to `nrows` in
 *     the datatable). This length must be nonnegative and cannot exceed
 *     2**63-1.
 *
 * min, max
 *     The smallest / largest elements in the current RowIndex. If the length
 *     of the rowindex is 0, then both min and max will be 0.
 *
 * ind32
 *     The array of (32-bit) indices comprising the RowIndex. The number of
 *     elements in this array is `length`. Available if `type == RI_ARR32`.
 *
 * ind64
 *     The array of (64-bit) indices comprising the RowIndex. The number of
 *     elements in this array is `length`. Available if `type == RI_ARR64`.
 *
 * slice.start
 * slice.step
 *     RowIndex specified as a slice (available if `type == RI_SLICE`). The
 *     indices in this RowIndex are:
 *         start, start + step, start + 2*step, ..., start + (length - 1)*step
 *     The `start` is a nonnegative number, while `step` may be positive,
 *     negative or zero (however `start + (length - 1)*step` must be nonnegative
 *     and being able to fit inside `int64_t`). The variable `start` is declared
 *     as `int64_t` because if it was `size_t` then it wouldn't be possible to
 *     do simple addition `start + step`.
 *
 * refcount
 *     Number of references to this RowIndex object. When this count reaches 0
 *     the object can be deallocated.
 */
class RowIndex
{
    int64_t _length;
    int64_t _min;
    int64_t _max;
  public:
    union {
      int32_t* ind32;
      int64_t* ind64;
      struct { int64_t start, step; } slice;
    };

  public:
    RowIndexType type;
    int32_t refcount;

  public:
    RowIndex(int64_t, int64_t, int64_t); // from slice
    RowIndex(int64_t*, int64_t*, int64_t*, int64_t); // from list of slices
    RowIndex(int64_t*, int64_t, int);
    RowIndex(int32_t*, int64_t, int);
    RowIndex(const RowIndex&);
    RowIndex(RowIndex*);
    RowIndex* expand();
    void compactify();
    size_t alloc_size();
    RowIndex* shallowcopy();
    void release();

    // Properties accessors
    int64_t length() const { return _length; }
    int64_t min() const { return _min; }
    int64_t max() const { return _max; }
    int64_t slice_start() const { return slice.start; }
    int64_t slice_step() const { return slice.step; }
    int32_t* indices32() const { return ind32; }
    int64_t* indices64() const { return ind64; }

    bool verify_integrity(IntegrityCheckContext&,
                          const std::string& name = "RowIndex") const;

    static RowIndex* merge(RowIndex*, RowIndex*);
    static RowIndex* from_intcolumn(Column*, int);
    static RowIndex* from_boolcolumn(Column*);
    static RowIndex* from_column(Column*);
    static RowIndex* from_filterfn32(rowindex_filterfn32*, int64_t, int);
    static RowIndex* from_filterfn64(rowindex_filterfn64*, int64_t, int);

  private:
    ~RowIndex();
};



//==============================================================================
// Public API
//==============================================================================
typedef RowIndex* (rowindex_getterfn)(void);


/**
 * Macro to get column indices based on a RowIndex `RI`.
 * Creates a for loop with from the following parameters:
 *
 *     `RI`:    A RowIndex pointer.
 *
 *     `NROWS`: The length of the target column (`Column::nrows`).
 *
 *     `I`:     A variable name that will be initialized as an `int64_t` and
 *              used to store the resulting index during each pass.
 *
 *     `CODE`:  The code to be placed in the body of the for loop.
 *
 * Two variables named `L_j` and `L_s` will also be created; Their
 * resulting type and value is undefined.
 */
#define DT_LOOP_OVER_ROWINDEX(I, NROWS, RI, CODE)                              \
  if (RI == nullptr) {                                                         \
    for (int64_t I = 0; I < NROWS; ++I) {                                      \
      CODE                                                                     \
    }                                                                          \
  } else if (RI->type == RI_SLICE) {                                           \
    int64_t I = RI->slice.start, L_s = RI->slice.step, L_j = 0;                \
    for (; L_j < NROWS; ++L_j, I += L_s) {                                     \
      CODE                                                                     \
    }                                                                          \
  } else if (RI->type == RI_ARR32) {                                           \
    int32_t* L_s = RI->ind32;                                                  \
    for (int64_t L_j = 0; L_j < NROWS; ++L_j) {                                \
      int64_t I = (int64_t) L_s[L_j];                                          \
      CODE                                                                     \
    }                                                                          \
  } else if (RI->type == RI_ARR64) {                                           \
    int64_t* L_s = RI->ind64;                                                  \
    for (int64_t L_j = 0; L_j < NROWS; ++L_j) {                                \
      int64_t I = L_s[L_j];                                                    \
      CODE                                                                     \
    }                                                                          \
  }


#endif
