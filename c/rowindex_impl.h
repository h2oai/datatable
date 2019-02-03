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
#ifndef dt_ROWINDEX_IMPL_h
#define dt_ROWINDEX_IMPL_h
#include "rowindex.h"



//------------------------------------------------------------------------------
// Base RowIndexImpl class
//------------------------------------------------------------------------------

class RowIndexImpl {
  public:
    /**
     * length
     *     The number of elements in the RowIndex.
     *
     * min, max
     *     Smallest / largest entry in the RowIndex. If the RowIndex is empty
     *     (length 0), or if all entries in the RowIndex are NAs, then
     *     min = max = RowIndex::NA.
     *
     * refcount
     *     Ref-counter for this RowIndexImpl object. A RowIndexImpl* object
     *     may be co-owned by several RowIndex objects, and `refcount` keeps
     *     track of the number of co-owners. When `refcount` reaches 0, the
     *     object is deleted.
     *
     * type
     *     The type of the RowIndex: SLICE, ARR32 or ARR64.
     *
     * ascending
     *     True if the entries in the rowindex are strictly increasing, or false
     *     otherwise. Note that if `ascending` is false it does not mean the
     *     elements are descending, they may also be non-monotonous.
     */
    size_t length;
    size_t min;
    size_t max;
    uint32_t refcount;
    RowIndexType type;
    bool ascending;
    int : 16;

  public:
    RowIndexImpl();
    RowIndexImpl(const RowIndexImpl&) = delete;
    RowIndexImpl(RowIndexImpl&&) = delete;
    RowIndexImpl& operator=(const RowIndexImpl&) = delete;
    RowIndexImpl& operator=(RowIndexImpl&&) = delete;
    virtual ~RowIndexImpl();

    RowIndexImpl* acquire();
    RowIndexImpl* release();

    virtual size_t nth(size_t i) const = 0;
    virtual RowIndexImpl* uplift_from(const RowIndexImpl*) const = 0;
    virtual RowIndexImpl* negate(size_t nrows) const = 0;

    virtual void resize(size_t n) = 0;
    virtual RowIndexImpl* resized(size_t n) = 0;

    virtual size_t memory_footprint() const = 0;
    virtual void verify_integrity() const;
};



//------------------------------------------------------------------------------
// "Slice" RowIndexImpl class
//------------------------------------------------------------------------------

class SliceRowIndexImpl : public RowIndexImpl {
  private:
    size_t start;
    size_t step;

  public:
    SliceRowIndexImpl(size_t start, size_t count, size_t step);

    size_t nth(size_t i) const override;
    RowIndexImpl* uplift_from(const RowIndexImpl*) const override;
    RowIndexImpl* negate(size_t nrows) const override;

    void resize(size_t n) override;
    RowIndexImpl* resized(size_t n) override;

    size_t memory_footprint() const override;
    void verify_integrity() const override;

  protected:
    friend RowIndex;
    friend size_t slice_rowindex_get_start(const RowIndexImpl*) noexcept;
    friend size_t slice_rowindex_get_step(const RowIndexImpl*) noexcept;
};


size_t slice_rowindex_get_start(const RowIndexImpl*) noexcept;
size_t slice_rowindex_get_step(const RowIndexImpl*) noexcept;
bool slice_rowindex_increasing(const RowIndexImpl*) noexcept;



//------------------------------------------------------------------------------
// "Array" RowIndexImpl class
//------------------------------------------------------------------------------

class ArrayRowIndexImpl : public RowIndexImpl {
  private:
    void* data;
    bool owned;
    size_t : 56;

  public:
    ArrayRowIndexImpl(arr32_t&& indices, bool sorted);
    ArrayRowIndexImpl(arr64_t&& indices, bool sorted);
    ArrayRowIndexImpl(arr32_t&& indices, size_t min, size_t max);
    ArrayRowIndexImpl(arr64_t&& indices, size_t min, size_t max);
    ArrayRowIndexImpl(const arr64_t& starts, const arr64_t& counts,
                      const arr64_t& steps);
    ArrayRowIndexImpl(filterfn32* f, size_t n, bool sorted);
    ArrayRowIndexImpl(filterfn64* f, size_t n, bool sorted);
    ArrayRowIndexImpl(const Column*);
    ~ArrayRowIndexImpl() override;

    const int32_t* indices32() const noexcept;
    const int64_t* indices64() const noexcept;

    size_t nth(size_t i) const override;
    RowIndexImpl* uplift_from(const RowIndexImpl*) const override;
    RowIndexImpl* negate(size_t nrows) const override;

    void resize(size_t n) override;
    RowIndexImpl* resized(size_t n) override;

    size_t memory_footprint() const override;
    void verify_integrity() const override;

  private:
    void _resize_data();
    void set_min_max();

    // Helper function that computes and sets proper `min` / `max` fields for
    // this RowIndex. The `sorted` flag is a hint whether the indices are
    // sorted (if they are, computing min/max is much simpler).
    template <typename T> void _set_min_max();

    // Helpers for `ArrayRowIndexImpl(Column*)`
    void init_from_boolean_column(const BoolColumn* col);
    void init_from_integer_column(const Column* col);
    void compactify();

    // Helper for `negate()`
    template <typename TI, typename TO>
    RowIndexImpl* negate_impl(size_t nrows) const;
};



#endif
