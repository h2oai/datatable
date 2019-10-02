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
 * nrows_
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
    size_t nrows_;
    SType  stype_;
    size_t : 56;
    mutable std::unique_ptr<Stats> stats;

  //------------------------------------
  // Constructors
  //------------------------------------
  public:
    ColumnImpl(size_t nrows, SType stype);
    ColumnImpl(const ColumnImpl&) = delete;
    ColumnImpl(ColumnImpl&&) = delete;
    virtual ~ColumnImpl() = default;

    ColumnImpl* acquire_instance() const;
    void release_instance() noexcept;


  //------------------------------------
  // Element access
  //------------------------------------
  public:
    virtual bool get_element(size_t i, int8_t* out) const;
    virtual bool get_element(size_t i, int16_t* out) const;
    virtual bool get_element(size_t i, int32_t* out) const;
    virtual bool get_element(size_t i, int64_t* out) const;
    virtual bool get_element(size_t i, float* out) const;
    virtual bool get_element(size_t i, double* out) const;
    virtual bool get_element(size_t i, CString* out) const;
    virtual bool get_element(size_t i, py::robj* out) const;


  //------------------------------------
  // Properties
  //------------------------------------
  public:
    size_t nrows() const noexcept { return nrows_; }
    SType  stype() const noexcept { return stype_; }
    virtual bool is_virtual() const noexcept = 0;

    virtual const void* data2() const { return nullptr; }

    virtual size_t memory_footprint() const noexcept;


  //------------------------------------
  // Data buffers
  //------------------------------------
  public:
    virtual NaStorage get_na_storage_method() const noexcept = 0;
    virtual size_t    get_num_data_buffers() const noexcept = 0;
    virtual bool        is_data_editable(size_t k) const = 0;
    virtual size_t      get_data_size(size_t k) const = 0;
    virtual const void* get_data_readonly(size_t k) const = 0;
    virtual void*       get_data_editable(size_t k) = 0;
    virtual Buffer      get_data_buffer(size_t k) const = 0;


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


    // Check that the data in this ColumnImpl object is correct.
    virtual void verify_integrity() const;

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

    virtual void fill_npmask(bool* outmask, size_t row0, size_t row1) const;

  protected:
    virtual void rbind_impl(colvec& columns, size_t nrows, bool isempty);

    template <typename T> ColumnImpl* _materialize_fw();
    ColumnImpl* _materialize_str();
    ColumnImpl* _materialize_obj();

    template <typename T>
    void _fill_npmask(bool* outmask, size_t row0, size_t row1) const;

    friend class Column;
    friend class dt::ConstNa_ColumnImpl;
};





#endif
