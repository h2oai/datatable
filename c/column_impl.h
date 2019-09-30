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
#ifndef dt_COLUMN_IMPL_h
#define dt_COLUMN_IMPL_h
#include <string>
#include <vector>
#include "python/list.h"
#include "python/obj.h"
#include "column.h"
#include "groupby.h"
#include "buffer.h"     // Buffer
#include "rowindex.h"
#include "stats.h"
#include "types.h"

class DataTable;
class BoolColumn;
class PyObjectColumn;
namespace dt {
  class ConstNa_ColumnImpl;
}
template <typename T> class IntColumn;
template <typename T> class StringColumn;

using colvec = std::vector<Column>;
using strvec = std::vector<std::string>;
using pimpl = std::unique_ptr<ColumnImpl>;



//==============================================================================

/**
 * A single column within a DataTable.
 *
 * A ColumnImpl is a self-sufficient object, i.e. it may exist outside of a
 * DataTable too. This usually happens when a DataTable is being transformed,
 * then new ColumnImpl objects will be created, manipulated, and eventually bundled
 * into a new DataTable object.
 *
 * The main "payload" of this class is the data buffer `mbuf`, which contains
 * a contiguous memory region with the column's data in NFF format. Columns
 * of "string" type carry an additional buffer `strbuf` that stores the column's
 * character data (while `mbuf` stores the offsets).
 *
 * The data buffer `mbuf` may be shared across multiple columns: this enables
 * light-weight "shallow" copying of ColumnImpl objects. A ColumnImpl may also reference
 * another ColumnImpl's `mbuf` applying a RowIndex `ri` to it. When a RowIndex is
 * present, it selects a subset of elements from `mbuf`, and only those
 * sub-elements are considered to be the actual values in the ColumnImpl.
 *
 * Parameters
 * ----------
 * mbuf
 *     Raw data buffer, generally it's a plain array of primitive C types
 *     (such as `int32_t` or `double`).
 *
 * _nrows
 *     Number of elements in this column. If the ColumnImpl has a rowindex, then
 *     this number will be the same as the number of elements in the rowindex.
 *
 * stats
 *     Auxiliary structure that contains stat values about this column, if
 *     they were computed.
 */
class ColumnImpl
{
  protected:
    Buffer mbuf;
    mutable std::unique_ptr<Stats> stats;
    size_t _nrows;
    SType _stype;
    size_t : 56;

  public:
    ColumnImpl(size_t nrows, SType stype);
    ColumnImpl(const ColumnImpl&) = delete;
    ColumnImpl(ColumnImpl&&) = delete;
    virtual ~ColumnImpl();

    ColumnImpl* acquire_instance() const;
    void release_instance() noexcept;

    virtual bool get_element(size_t i, int8_t* out) const;
    virtual bool get_element(size_t i, int16_t* out) const;
    virtual bool get_element(size_t i, int32_t* out) const;
    virtual bool get_element(size_t i, int64_t* out) const;
    virtual bool get_element(size_t i, float* out) const;
    virtual bool get_element(size_t i, double* out) const;
    virtual bool get_element(size_t i, CString* out) const;
    virtual bool get_element(size_t i, py::robj* out) const;

    virtual bool is_virtual() const noexcept { return false; }

    size_t nrows() const { return _nrows; }
    SType stype() const { return _stype; }
    LType ltype() const { return info(_stype).ltype(); }
    const Buffer& data_buf() const { return mbuf; }
    const void* data() const { return mbuf.rptr(); }
    virtual const void* data2() const { return nullptr; }
    virtual size_t data2_size() const { return 0; }
    void* data_w() {
      xassert(!is_virtual());
      return mbuf.wptr();
    }

    virtual size_t data_nrows() const;
    virtual size_t memory_footprint() const;

    RowIndex _sort(Groupby* out_groups) const;
    virtual void sort_grouped(const Groupby&, Column& out);

    /**
      * Repeat the column `ntimes` times. The implementation may either
      * modify the current column (if it can), or otherwise it should
      * create a new instance and store it in the provided `out` object.
      *
      * Implementation in column/repeated.cc
      */
    virtual void repeat(size_t ntimes, Column& out);

    virtual void na_pad(size_t new_nrows, Column& out);
    virtual void truncate(size_t new_nrows, Column& out);

    /**
      * Implementation in column/view.cc
      */
    virtual void apply_rowindex(const RowIndex& ri, Column& out);

    /**
     * Modify the ColumnImpl, replacing values specified by the provided `mask` with
     * NAs. The `mask` column must have the same number of rows as the current,
     * and neither of them can have a RowIndex.
     */
    virtual void apply_na_mask(const Column& mask);

    /**
     * Create a shallow copy of this ColumnImpl, possibly applying the provided
     * RowIndex. The copy is "shallow" in the sense that the main data buffer
     * is copied by-reference. If this column has a rowindex, and the user
     * asks to apply a new rowindex, then the new one will replace the original.
     * If you want the rowindices to be merged, you should merge them manually
     * and pass the merged rowindex to this method.
     */
    virtual ColumnImpl* shallowcopy() const = 0;

    /**
     * Replace values at positions given by the RowIndex `replace_at` with
     * values taken from the ColumnImpl `replace_with`. The ltype of the replacement
     * column should be compatible with the current, and its number of rows
     * should be either 1 or equal to the length of `replace_at` (which must not
     * be empty).
     * The values are replaced in-place, if possible (if reference count is 1),
     * or otherwise the copy of a column is created and returned, and the
     * current ColumnImpl object is deleted.
     *
     * If the `replace_with` column is nullptr, then the values will be replaced
     * with NAs.
     */
    virtual void replace_values(
        Column& thiscol,
        const RowIndex& replace_at,
        const Column& replace_with);

    /**
     * Appends the provided columns to the bottom of the current column and
     * returns the resulting column. This method is equivalent to `list.append()`
     * in Python or `rbind()` in R.
     *
     * Current column is modified in-place, if possible. Otherwise, a new ColumnImpl
     * object is returned, and this ColumnImpl is deleted. The expected usage pattern
     * is thus as follows:
     *
     *   column = column->rbind(columns_to_bind);
     *
     * Individual entries in the `columns` array may be instances of `VoidColumn`,
     * indicating columns that should be replaced with NAs.
     */
    // ColumnImpl* rbind(colvec& columns);

    /**
     * "Materialize" the ColumnImpl. Depending on the column, this
     * could be either done in-place, or a new ColumnImpl must be
     * created to replace the current. In the former case, `this`
     * is returned; in the latter we return the new instance and
     * the current instance is released. Thus, the expected semantics
     * of using this method is:
     *
     *     pcol = pcol->materialize();
     *
     */
  public:
    virtual ColumnImpl* materialize();
    virtual void pre_materialize_hook() {}


    /**
     * Check that the data in this ColumnImpl object is correct. `name` is the name of
     * the column to be used in the diagnostic messages.
     */
  public:
    virtual void verify_integrity(const std::string& name) const;

    /**
     * get_stats()
     *   Getter method for this column's reference to `Stats`. If the reference
     *   is null then the method will create a new Stats instance for this column
     *   and return a pointer to said instance.
     *
     * get_stats_if_exist()
     *   A simpler accessor than get_stats(): this will return either an existing
     *   Stats object, or nullptr if the Stats were not initialized yet.
     */
    Stats* get_stats_if_exist() const { return stats.get(); }  // REMOVE

    virtual void fill_na_mask(int8_t* outmask, size_t row0, size_t row1);

  protected:
    virtual void init_data();
    virtual void rbind_impl(colvec& columns, size_t nrows, bool isempty);

    friend class Column;
    friend class dt::ConstNa_ColumnImpl;
};






//==============================================================================
// String column
//==============================================================================

template <typename T> class StringColumn : public ColumnImpl
{
  Buffer strbuf;

public:
  StringColumn();
  StringColumn(size_t nrows);
  StringColumn(size_t nrows, Buffer&& offbuf, Buffer&& strbuf);

  ColumnImpl* materialize() override;
  void apply_na_mask(const Column& mask) override;

  Buffer str_buf() const { return strbuf; }
  size_t datasize() const;
  size_t data_nrows() const override;
  const char* strdata() const;
  const uint8_t* ustrdata() const;
  const T* offsets() const;
  T* offsets_w();
  size_t memory_footprint() const override;
  const void* data2() const override { return strbuf.rptr(); }
  size_t data2_size() const override {
    return static_cast<const T*>(mbuf.rptr())[_nrows] & ~GETNA<T>();
  }

  ColumnImpl* shallowcopy() const override;
  void replace_values(Column& thiscol, const RowIndex& at, const Column& with) override;

  void verify_integrity(const std::string& name) const override;

  bool get_element(size_t i, CString* out) const override;

protected:
  void init_data() override;

  void rbind_impl(colvec& columns, size_t nrows, bool isempty) override;

  friend ColumnImpl;
  friend Column;
};


extern template class StringColumn<uint32_t>;
extern template class StringColumn<uint64_t>;


#endif
