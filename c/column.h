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
#ifndef dt_COLUMN_h
#define dt_COLUMN_h
#include <string>
#include <vector>
#include "groupby.h"
#include "memrange.h"     // MemoryRange
#include "python/list.h"
#include "python/obj.h"
#include "rowindex.h"
#include "stats.h"
#include "types.h"

class DataTable;
class BoolColumn;
class PyObjectColumn;
class FreadReader;  // used as a friend
class iterable;     // helper for Column::from_py_iterable
template <typename T> class IntColumn;
template <typename T> class RealColumn;
template <typename T> class StringColumn;

using colvec = std::vector<OColumn>;
using strvec = std::vector<std::string>;

/**
 * Helper templates to convert between an stype and a column's type:
 *
 * column_t<stype>
 *   resolves to the type of the column that implements `stype`.
 *
 * element_t<stype>
 *   resolves to the type of the element that is in the main data buffer
 *   of `column_t<stype>`.
 */
template <SType s> struct _colt {};
template <> struct _colt<SType::BOOL>    { using t = BoolColumn; };
template <> struct _colt<SType::INT8>    { using t = IntColumn<int8_t>; };
template <> struct _colt<SType::INT16>   { using t = IntColumn<int16_t>; };
template <> struct _colt<SType::INT32>   { using t = IntColumn<int32_t>; };
template <> struct _colt<SType::INT64>   { using t = IntColumn<int64_t>; };
template <> struct _colt<SType::FLOAT32> { using t = RealColumn<float>; };
template <> struct _colt<SType::FLOAT64> { using t = RealColumn<double>; };
template <> struct _colt<SType::STR32>   { using t = StringColumn<uint32_t>; };
template <> struct _colt<SType::STR64>   { using t = StringColumn<uint64_t>; };
template <> struct _colt<SType::OBJ>     { using t = PyObjectColumn; };

template <SType s>
using column_t = typename _colt<s>::t;

template <SType s> struct _elt {};
template <> struct _elt<SType::BOOL>    { using t = int8_t; };
template <> struct _elt<SType::INT8>    { using t = int8_t; };
template <> struct _elt<SType::INT16>   { using t = int16_t; };
template <> struct _elt<SType::INT32>   { using t = int32_t; };
template <> struct _elt<SType::INT64>   { using t = int64_t; };
template <> struct _elt<SType::FLOAT32> { using t = float; };
template <> struct _elt<SType::FLOAT64> { using t = double; };
template <> struct _elt<SType::STR32>   { using t = uint32_t; };
template <> struct _elt<SType::STR64>   { using t = uint64_t; };
template <> struct _elt<SType::OBJ>     { using t = PyObject*; };

template <SType s>
using element_t = typename _elt<s>::t;

template <typename T> struct _promote { using t = T; };
template <> struct _promote<int8_t>    { using t = int32_t; };
template <> struct _promote<int16_t>   { using t = int32_t; };
template <> struct _promote<PyObject*> { using t = py::robj; };

template <typename T>
using promote = typename _promote<T>::t;

template <SType s>
using getelement_t = promote<element_t<s>>;

template <typename T>
inline T downcast(promote<T> v) {
  return ISNA<promote<T>>(v)? GETNA<T>() : static_cast<T>(v);
}

template <>
inline PyObject* downcast(py::robj v) {
  return v.to_borrowed_ref();
}



//==============================================================================

/**
 * A single column within a DataTable.
 *
 * A Column is a self-sufficient object, i.e. it may exist outside of a
 * DataTable too. This usually happens when a DataTable is being transformed,
 * then new Column objects will be created, manipulated, and eventually bundled
 * into a new DataTable object.
 *
 * The main "payload" of this class is the data buffer `mbuf`, which contains
 * a contiguous memory region with the column's data in NFF format. Columns
 * of "string" type carry an additional buffer `strbuf` that stores the column's
 * character data (while `mbuf` stores the offsets).
 *
 * The data buffer `mbuf` may be shared across multiple columns: this enables
 * light-weight "shallow" copying of Column objects. A Column may also reference
 * another Column's `mbuf` applying a RowIndex `ri` to it. When a RowIndex is
 * present, it selects a subset of elements from `mbuf`, and only those
 * sub-elements are considered to be the actual values in the Column.
 *
 * Parameters
 * ----------
 * mbuf
 *     Raw data buffer, generally it's a plain array of primitive C types
 *     (such as `int32_t` or `double`).
 *
 * ri
 *     RowIndex applied to the column's data. All access to the contents of the
 *     Column should go through the rowindex. For example, the `i`-th element
 *     in the column can be found as `mbuf->get_elem<T>(ri[i])`.
 *     This may also be NULL, which is equivalent to being an identity rowindex.
 *
 * nrows
 *     Number of elements in this column. If the Column has a rowindex, then
 *     this number will be the same as the number of elements in the rowindex.
 *
 * stats
 *     Auxiliary structure that contains stat values about this column, if
 *     they were computed.
 */
class Column
{
protected:
  MemoryRange mbuf;
  RowIndex ri;
  mutable std::unique_ptr<Stats> stats;
  size_t _nrows;
  SType _stype;
  size_t : 56;

public:
  Column(const Column&) = delete;
  Column(Column&&) = delete;
  virtual ~Column();

  virtual bool get_element(size_t i, int32_t* out) const;
  virtual bool get_element(size_t i, int64_t* out) const;
  virtual bool get_element(size_t i, float* out) const;
  virtual bool get_element(size_t i, double* out) const;
  virtual bool get_element(size_t i, CString* out) const;
  virtual bool get_element(size_t i, py::robj* out) const;

  const RowIndex& rowindex() const noexcept { return ri; }
  RowIndex remove_rowindex();
  void replace_rowindex(const RowIndex& newri);

  size_t nrows() const { return _nrows; }
  SType stype() const { return _stype; }
  LType ltype() const { return info(_stype).ltype(); }
  const MemoryRange& data_buf() const { return mbuf; }
  const void* data() const { return mbuf.rptr(); }
  virtual const void* data2() const { return nullptr; }
  virtual size_t data2_size() const { return 0; }
  void* data_w() { return mbuf.wptr(); }
  PyObject* mbuf_repr() const;
  size_t alloc_size() const;

  virtual size_t data_nrows() const = 0;
  virtual size_t memory_footprint() const;

  RowIndex _sort(Groupby* out_groups) const;

  /**
   * Resize the column up to `nrows` elements, and fill all new elements with
   * NA values, except when the Column initially had just one row, in which case
   * that one value will be used to fill the new rows. For example:
   *
   *   {1, 2, 3, 4}.resize_and_fill(5) -> {1, 2, 3, 4, NA}
   *   {1, 3}.resize_and_fill(5)       -> {1, 3, NA, NA, NA}
   *   {1}.resize_and_fill(5)          -> {1, 1, 1, 1, 1}
   *
   * The contents of the column will be modified in-place if possible.
   *
   * This method can be used to both increase and reduce the size of the
   * column.
   */
  virtual void resize_and_fill(size_t nrows) = 0;
  OColumn repeat(size_t nreps) const;

  /**
   * Modify the Column, replacing values specified by the provided `mask` with
   * NAs. The `mask` column must have the same number of rows as the current,
   * and neither of them can have a RowIndex.
   */
  virtual void apply_na_mask(const OColumn& mask) = 0;

  /**
   * Create a shallow copy of this Column, possibly applying the provided
   * RowIndex. The copy is "shallow" in the sense that the main data buffer
   * is copied by-reference. If this column has a rowindex, and the user
   * asks to apply a new rowindex, then the new one will replace the original.
   * If you want the rowindices to be merged, you should merge them manually
   * and pass the merged rowindex to this method.
   */
  virtual Column* shallowcopy() const;

  /**
   * Factory method to cast the current column into the given `stype`. If a
   * column is cast into its own stype, a shallow copy is returned. Otherwise,
   * this method constructs a new column of the provided stype and writes the
   * converted data into it.
   *
   * If the MemoryRange is provided, then that buffer will be used in the
   * creation of the resulting column (the Column will assume ownership of the
   * provided MemoryRange).
   */

  /**
   * Replace values at positions given by the RowIndex `replace_at` with
   * values taken from the Column `replace_with`. The ltype of the replacement
   * column should be compatible with the current, and its number of rows
   * should be either 1 or equal to the length of `replace_at` (which must not
   * be empty).
   * The values are replaced in-place, if possible (if reference count is 1),
   * or otherwise the copy of a column is created and returned, and the
   * current Column object is deleted.
   *
   * If the `replace_with` column is nullptr, then the values will be replaced
   * with NAs.
   */
  virtual void replace_values(
      OColumn& thiscol,
      const RowIndex& replace_at,
      const OColumn& replace_with) = 0;

  /**
   * Appends the provided columns to the bottom of the current column and
   * returns the resulting column. This method is equivalent to `list.append()`
   * in Python or `rbind()` in R.
   *
   * Current column is modified in-place, if possible. Otherwise, a new Column
   * object is returned, and this Column is deleted. The expected usage pattern
   * is thus as follows:
   *
   *   column = column->rbind(columns_to_bind);
   *
   * Individual entries in the `columns` array may be instances of `VoidColumn`,
   * indicating columns that should be replaced with NAs.
   */
  // Column* rbind(colvec& columns);

  /**
   * "Materialize" the Column. If the Column has no rowindex, this is a no-op.
   * Otherwise, this method "applies" the rowindex to the column's data and
   * subsequently replaces the column's data buffer with a new one that contains
   * "plain" data. The rowindex object is subsequently released, and the Column
   * becomes converted from "view column" into a "data column".
   *
   * This operation is in-place, and we attempt to reuse existing memory buffer
   * whenever possible.
   *
   * If the Column's rowindex carries groupby information, then we retain it
   * by replacing the current rowindex with the "plain slice" (i.e. a slice
   * with step 1).
   */
  virtual void materialize() = 0;


  /**
   * Check that the data in this Column object is correct. `name` is the name of
   * the column to be used in the diagnostic messages.
   */
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

  virtual void fill_na_mask(int8_t* outmask, size_t row0, size_t row1) = 0;

protected:
  Column(size_t nrows = 0);
  virtual void init_data() = 0;
  virtual void rbind_impl(colvec& columns, size_t nrows, bool isempty) = 0;

  /**
   * Sets every row in the column to an NA value. As of now this method
   * modifies every element in the column's memory buffer regardless of its
   * refcount or rowindex. Use with caution.
   * This implementation will be made safer after Column::extract is modified
   * to be an in-place operation.
   */
  virtual void fill_na() = 0;

  friend class OColumn;
};



//------------------------------------------------------------------------------
// OColumn
//------------------------------------------------------------------------------

// "owner" of a Column
// Eventually will be renamed into simple Column
class OColumn
{
  private:
    // shared pointer; its lifetime is governed by `pcol->refcount`: when the
    // reference count reaches 0, the object is deleted.
    // We do not use std::shared_ptr<> here primarily because it makes it much
    // harder to inspect the object in GDB / LLDB.
    Column* pcol;

  public:
    static constexpr size_t MAX_ARR32_SIZE = 0x7FFFFFFF;

  //------------------------------------
  // Constructors
  //------------------------------------
  public:
    OColumn();
    OColumn(const OColumn&);
    OColumn(OColumn&&);
    OColumn& operator=(const OColumn&);
    OColumn& operator=(OColumn&&);
    ~OColumn();

    static OColumn new_data_column(SType, size_t nrows);
    static OColumn new_na_column(SType, size_t nrows);
    static OColumn new_mbuf_column(SType, MemoryRange&&);
    static OColumn from_buffer(const py::robj& buffer);
    static OColumn from_pylist(const py::olist& list, int stype0 = 0);
    static OColumn from_pylist_of_tuples(const py::olist& list, size_t index, int stype0);
    static OColumn from_pylist_of_dicts(const py::olist& list, py::robj name, int stype0);
    static OColumn from_range(int64_t start, int64_t stop, int64_t step, SType);
    static OColumn from_strvec(const strvec&);

  private:
    // Assumes ownership of the `col` object
    explicit OColumn(Column* col);

  //------------------------------------
  // Properties
  //------------------------------------
  public:
    size_t nrows() const noexcept;
    size_t na_count() const;
    SType stype() const noexcept;
    LType ltype() const noexcept;
    bool is_fixedwidth() const noexcept;
    bool is_virtual() const noexcept;
    size_t elemsize() const noexcept;

    operator bool() const noexcept;

    // TEMP accessors to the underlying implementation
    const Column* get() const { return pcol; }

    Column* operator->();
    const Column* operator->() const;


  //------------------------------------
  // Data access
  //------------------------------------
  public:
    // Each `get_element(i, &out)` function retrieves the column's element
    // at index `i` and stores it in the variable `out`. The return value
    // is true if the returned element is NA (missing), or false otherwise.
    // When `true` is returned, the `out` value may contain garbage data,
    // and should not be used in any way.
    //
    // Multiple overloads of `get_element()` correspond to different stypes
    // of the underlying column. It is the caller's responsibility to call
    // the correct function variant; calling a method that doesn't match
    // this column's SType will likely result in an exception thrown.
    //
    // The function expects (but doesn't check) that `i < nrows`. A segfault
    // may occur if this assumption is violated.
    //
    bool get_element(size_t i, int32_t* out) const;
    bool get_element(size_t i, int64_t* out) const;
    bool get_element(size_t i, float* out) const;
    bool get_element(size_t i, double* out) const;
    bool get_element(size_t i, CString* out) const;
    bool get_element(size_t i, py::robj* out) const;

    // `get_element_as_pyobject(i)` returns the i-th element of the column
    // wrapped into a pyobject of the appropriate type. Use this function
    // for interoperation with Python.
    //
    py::oobj get_element_as_pyobject(size_t i) const;

    // Access to the underlying column's data. If the column is virtual,
    // it will be materialized first. The index `i` allows access to
    // additional data buffers, depending on the column's type.
    //
    // For example, for string columns `get_data_readonly(0)` returns the
    // array of 'start' offsets, while `get_data_readonly(1)` returns the
    // raw character buffer.
    //
    // These methods are not marked const, because they may cause the
    // column to become materialized.
    //
    const void* get_data_readonly(size_t i = 0);
    void* get_data_editable();
    size_t get_data_size(size_t i = 0);


  //------------------------------------
  // Stats
  //------------------------------------
  public:
    // Each column may optionally carry a `Stats` object which contains
    // various summary statistics about the column: sum, mean, min, max, etc.
    // Methods `stats()` will return a (borrowed) pointer to that stats object,
    // instantiating it if necessary, while `get_stats_if_exists()` will
    // return nullptr if the Stats object has not been initialized yet.
    //
    // Most stats functions can be accessed directly via the returns Stats
    // object, however OColumn provides two additional helper functions:
    //
    //   reset_stats()
    //     Will clear the current Stats object if it exists, or do nothing
    //     if it doesn't;
    //
    //   is_stat_computed(stat)
    //     Will return true if Stats object exists and the provided `stat`
    //     has been already computed, or false otherwise.
    //
    Stats* stats() const;
    Stats* get_stats_if_exist() const;
    void reset_stats();
    bool is_stat_computed(Stat) const;


  //------------------------------------
  // Column manipulation
  //------------------------------------
  public:
    void rbind(colvec& columns);
    void materialize();
    OColumn cast(SType stype) const;
    OColumn cast(SType stype, MemoryRange&& mr) const;
    RowIndex sort(Groupby* out_groups) const;
    RowIndex sort_grouped(const RowIndex&, const Groupby&) const;

    void replace_values(const RowIndex& replace_at, const OColumn& replace_with);

    friend void swap(OColumn& lhs, OColumn& rhs);
    friend OColumn new_string_column(size_t, MemoryRange&&, MemoryRange&&);
    friend class Column;
};




//==============================================================================

template <typename T> class FwColumn : public Column
{
public:
  FwColumn();
  FwColumn(size_t nrows);
  FwColumn(size_t nrows, MemoryRange&&);
  const T* elements_r() const;
  T* elements_w();
  T get_elem(size_t i) const;

  size_t data_nrows() const override;
  void resize_and_fill(size_t nrows) override;
  void apply_na_mask(const OColumn& mask) override;
  virtual void materialize() override;
  void replace_values(OColumn& thiscol, const RowIndex& at, const OColumn& with) override;
  void replace_values(const RowIndex& at, T with);
  void fill_na_mask(int8_t* outmask, size_t row0, size_t row1) override;

protected:
  void init_data() override;
  static constexpr T na_elem = GETNA<T>();
  void rbind_impl(colvec& columns, size_t nrows, bool isempty) override;
  void fill_na() override;

  friend Column;
};


extern template class FwColumn<int8_t>;
extern template class FwColumn<int16_t>;
extern template class FwColumn<int32_t>;
extern template class FwColumn<int64_t>;
extern template class FwColumn<float>;
extern template class FwColumn<double>;
extern template class FwColumn<PyObject*>;



//==============================================================================

class BoolColumn : public FwColumn<int8_t>
{
public:
  BoolColumn(size_t nrows = 0);
  BoolColumn(size_t nrows, MemoryRange&&);

  bool get_element(size_t i, int32_t* out) const override;

  protected:

  void verify_integrity(const std::string& name) const override;

  using Column::mbuf;
  friend Column;
};



//==============================================================================

template <typename T> class IntColumn : public FwColumn<T>
{
public:
  IntColumn(size_t nrows = 0);
  IntColumn(size_t nrows, MemoryRange&&);

  bool get_element(size_t i, int32_t* out) const override;
  bool get_element(size_t i, int64_t* out) const override;

protected:
  using Column::stats;
  using Column::mbuf;
  friend Column;
};

extern template class IntColumn<int8_t>;
extern template class IntColumn<int16_t>;
extern template class IntColumn<int32_t>;
extern template class IntColumn<int64_t>;


//==============================================================================

template <typename T> class RealColumn : public FwColumn<T>
{
public:
  RealColumn(size_t nrows = 0);
  RealColumn(size_t nrows, MemoryRange&&);

  bool get_element(size_t i, T* out) const override;

protected:
  using Column::stats;
  friend Column;
};

extern template class RealColumn<float>;
extern template class RealColumn<double>;



//==============================================================================

/**
 * Column containing `PyObject*`s.
 *
 * This column is a fall-back for implementing types that cannot be normally
 * supported by other columns. Manipulations with this column almost invariably
 * go through Python runtime, and hence are single-threaded and slow.
 *
 * The `mbuf` array for this Column must be marked as "pyobjects" (see
 * documentation for MemoryRange). In practice it means that:
 *   * Only real python objects may be stored, not NULL pointers.
 *   * All stored `PyObject*`s must have their reference counts incremented.
 *   * When a value is removed or replaced in `mbuf`, it should be decref'd.
 * The `mbuf`'s API already respects these rules, however the user must also
 * obey them when manipulating the data manually.
 */
class PyObjectColumn : public FwColumn<PyObject*>
{
public:
  PyObjectColumn();
  PyObjectColumn(size_t nrows);
  PyObjectColumn(size_t nrows, MemoryRange&&);

  bool get_element(size_t i, py::robj* out) const override;

protected:

  void rbind_impl(colvec& columns, size_t nrows, bool isempty) override;

  void resize_and_fill(size_t nrows) override;
  void fill_na() override;
  void materialize() override;
  void verify_integrity(const std::string& name) const override;

  friend Column;
};



//==============================================================================
// String column
//==============================================================================

template <typename T> class StringColumn : public Column
{
  MemoryRange strbuf;

public:
  StringColumn();
  StringColumn(size_t nrows);

  void materialize() override;
  void resize_and_fill(size_t nrows) override;
  void apply_na_mask(const OColumn& mask) override;

  MemoryRange str_buf() const { return strbuf; }
  size_t datasize() const;
  size_t data_nrows() const override;
  const char* strdata() const;
  const uint8_t* ustrdata() const;
  const T* offsets() const;
  T* offsets_w();
  size_t memory_footprint() const override;
  const void* data2() const override { return strbuf.rptr(); }
  size_t data2_size() const override { return strbuf.size(); }

  Column* shallowcopy() const override;
  void replace_values(OColumn& thiscol, const RowIndex& at, const OColumn& with) override;

  void verify_integrity(const std::string& name) const override;

  bool get_element(size_t i, CString* out) const override;
  void fill_na_mask(int8_t* outmask, size_t row0, size_t row1) override;

protected:
  StringColumn(size_t nrows, MemoryRange&& offbuf, MemoryRange&& strbuf);
  void init_data() override;

  void rbind_impl(colvec& columns, size_t nrows, bool isempty) override;

  void fill_na() override;

  friend Column;
  friend FreadReader;  // friend Column* alloc_column(SType, size_t, int);
  friend OColumn new_string_column(size_t, MemoryRange&&, MemoryRange&&);
};


/**
 * Use this function to create a column from existing offsets & strdata.
 * It will create either StringColumn<uint32_t>* or StringColumn<uint64_t>*
 * depending on the size of the data.
 */
OColumn new_string_column(size_t n, MemoryRange&& data, MemoryRange&& str);


extern template class StringColumn<uint32_t>;
extern template class StringColumn<uint64_t>;



//==============================================================================

// "Fake" column, its only use is to serve as a placeholder for a Column with an
// unknown type. This column cannot be put into a DataTable.
class VoidColumn : public Column {
  public:
    VoidColumn();
    VoidColumn(size_t nrows);
    size_t data_nrows() const override;
    void materialize() override;
    void resize_and_fill(size_t) override;
    void rbind_impl(colvec&, size_t, bool) override;
    void apply_na_mask(const OColumn&) override;
    void replace_values(OColumn&, const RowIndex&, const OColumn&) override;
    void fill_na_mask(int8_t* outmask, size_t row0, size_t row1) override;
  protected:
    void init_data() override;
    void fill_na() override;

    friend Column;
};




#endif
