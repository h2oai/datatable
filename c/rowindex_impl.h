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
    RowIndexType type;
    bool ascending;
    size_t : 16;
    uint32_t refcount;
    size_t length;
    size_t min;
    size_t max;

    RowIndexImpl();
    RowIndexImpl(const RowIndexImpl&) = delete;
    RowIndexImpl(RowIndexImpl&&) = delete;
    RowIndexImpl& operator=(const RowIndexImpl&) = delete;
    RowIndexImpl& operator=(RowIndexImpl&&) = delete;

    void acquire();
    void release();

    virtual size_t nth(size_t i) const = 0;
    virtual RowIndexImpl* uplift_from(const RowIndexImpl*) = 0;
    virtual RowIndexImpl* inverse(size_t nrows) const = 0;
    virtual void shrink(size_t n) = 0;
    virtual RowIndexImpl* shrunk(size_t n) = 0;
    virtual size_t memory_footprint() const = 0;
    virtual void verify_integrity() const;

  protected:
    virtual ~RowIndexImpl();
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
    RowIndexImpl* uplift_from(const RowIndexImpl*) override;
    RowIndexImpl* inverse(size_t nrows) const override;
    void shrink(size_t n) override;
    RowIndexImpl* shrunk(size_t n) override;
    size_t memory_footprint() const override;
    void verify_integrity() const override;

  protected:
    friend RowIndex;
    friend size_t slice_rowindex_get_start(const RowIndexImpl*);
    friend size_t slice_rowindex_get_step(const RowIndexImpl*);
};


size_t slice_rowindex_get_start(const RowIndexImpl*);
size_t slice_rowindex_get_step(const RowIndexImpl*);
bool slice_rowindex_increasing(const RowIndexImpl*);



//------------------------------------------------------------------------------
// "Array" RowIndexImpl class
//------------------------------------------------------------------------------

class ArrayRowIndexImpl : public RowIndexImpl {
  private:
    arr32_t ind32;
    arr64_t ind64;
    bool is_sorted;
    int64_t : 56;

  public:
    ArrayRowIndexImpl(arr32_t&& indices, bool sorted);
    ArrayRowIndexImpl(arr64_t&& indices, bool sorted);
    ArrayRowIndexImpl(const arr64_t& starts, const arr64_t& counts,
                      const arr64_t& steps);
    ArrayRowIndexImpl(filterfn32* f, int64_t n, bool sorted);
    ArrayRowIndexImpl(filterfn64* f, int64_t n, bool sorted);
    ArrayRowIndexImpl(Column*);

    size_t nth(size_t i) const override;
    const int32_t* indices32() const { return ind32.data(); }
    const int64_t* indices64() const { return ind64.data(); }
    RowIndexImpl* uplift_from(const RowIndexImpl*) override;
    RowIndexImpl* inverse(size_t nrows) const override;
    void shrink(size_t n) override;
    RowIndexImpl* shrunk(size_t n) override;
    size_t memory_footprint() const override;
    void verify_integrity() const override;

  private:
    // Helper function that computes and sets proper `min` / `max` fields for
    // this RowIndex. The `sorted` flag is a hint whether the indices are
    // sorted (if they are, computing min/max is much simpler).
    template <typename T> void set_min_max(const dt::array<T>&);

    // Helpers for `ArrayRowIndexImpl(Column*)`
    void init_from_boolean_column(BoolColumn* col);
    void init_from_integer_column(Column* col);
    void compactify();

    // Helper for `inverse()`
    template <typename TI, typename TO>
    RowIndexImpl* inverse_impl(const dt::array<TI>& inp, size_t nrows) const;
};



#endif
