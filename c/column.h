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
#include <Python.h>
#include "groupby.h"
#include "memrange.h"     // MemoryRange
#include "py_types.h"
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
  mutable Stats* stats;

public:  // TODO: convert this into private
  size_t nrows;

public:
  static constexpr size_t MAX_STRING_SIZE = 0x7FFFFFFF;
  static constexpr size_t MAX_STR32_BUFFER_SIZE = 0x7FFFFFFF;
  static constexpr size_t MAX_STR32_NROWS = 0x7FFFFFFF;

  static Column* new_data_column(SType, size_t nrows);
  static Column* new_na_column(SType, size_t nrows);
  static Column* new_mmap_column(SType, size_t nrows, const std::string& filename);
  static Column* open_mmap_column(SType, size_t nrows, const std::string& filename,
                                  bool recode = false);
  static Column* new_xbuf_column(SType, size_t nrows, Py_buffer* pybuffer);
  static Column* new_mbuf_column(SType, MemoryRange&&);
  static Column* from_pylist(const py::olist& list, int stype0 = 0);
  static Column* from_pylist_of_tuples(const py::olist& list, size_t index, int stype0);
  static Column* from_pylist_of_dicts(const py::olist& list, py::robj name, int stype0);
  static Column* from_buffer(const py::robj& buffer);
  static Column* from_range(int64_t start, int64_t stop, int64_t step, SType s);

  Column(const Column&) = delete;
  Column(Column&&) = delete;
  virtual ~Column();
  virtual void replace_buffer(MemoryRange&&);
  virtual void replace_buffer(MemoryRange&&, MemoryRange&&);

  virtual SType stype() const noexcept = 0;
  virtual size_t elemsize() const = 0;
  virtual bool is_fixedwidth() const = 0;
  LType ltype() const noexcept { return info(stype()).ltype(); }

  const RowIndex& rowindex() const noexcept { return ri; }
  RowIndex remove_rowindex();
  void replace_rowindex(const RowIndex& newri);

  MemoryRange data_buf() const { return mbuf; }
  const void* data() const { return mbuf.rptr(); }
  void* data_w() { return mbuf.wptr(); }
  PyObject* mbuf_repr() const;
  size_t alloc_size() const;

  virtual size_t data_nrows() const = 0;
  size_t memory_footprint() const;

  RowIndex sort(Groupby* out_groups) const;

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
  Column* repeat(size_t nreps);

  /**
   * Modify the Column, replacing values specified by the provided `mask` with
   * NAs. The `mask` column must have the same number of rows as the current,
   * and neither of them can have a RowIndex.
   */
  virtual void apply_na_mask(const BoolColumn* mask) = 0;

  /**
   * Create a shallow copy of this Column, possibly applying the provided
   * RowIndex. The copy is "shallow" in the sense that the main data buffer
   * is copied by-reference. If this column has a rowindex, and the user
   * asks to apply a new rowindex, then the new one will replace the original.
   * If you want the rowindices to be merged, you should merge them manually
   * and pass the merged rowindex to this method.
   */
  virtual Column* shallowcopy(const RowIndex& new_rowindex) const;
  Column* shallowcopy() const { return shallowcopy(RowIndex()); }

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
  Column* cast(SType stype) const;
  Column* cast(SType stype, MemoryRange&& mr) const;

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
    RowIndex replace_at, const Column* replace_with) = 0;

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
  Column* rbind(std::vector<const Column*>& columns);

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
  virtual void reify() = 0;

  virtual py::oobj get_value_at_index(size_t i) const = 0;

  virtual RowIndex join(const Column* keycol) const = 0;

  virtual void save_to_disk(const std::string&, WritableBuffer::Strategy);

  size_t countna() const;
  size_t nunique() const;
  size_t nmodal() const;
  virtual int64_t min_int64() const { return GETNA<int64_t>(); }
  virtual int64_t max_int64() const { return GETNA<int64_t>(); }

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
  virtual Stats* get_stats() const = 0;
  Stats* get_stats_if_exist() const { return stats; }

  virtual void fill_na_mask(int8_t* outmask, size_t row0, size_t row1) = 0;

protected:
  Column(size_t nrows = 0);
  virtual void init_data() = 0;
  virtual void init_mmap(const std::string& filename) = 0;
  virtual void open_mmap(const std::string& filename, bool recode) = 0;
  virtual void init_xbuf(Py_buffer* pybuffer) = 0;
  virtual void rbind_impl(std::vector<const Column*>& columns,
                          size_t nrows, bool isempty) = 0;

  /**
   * These functions are designed to cast the current column into another type.
   * Each of the functions takes as an argument the new column object which
   * ought to be filled with the converted data. The `cast_into(...)` functions
   * do not modify the current column.
   *
   * The argument to the `cast_into(...)` methods is the "target" column - a
   * new writable column preallocated for `nrows` elements.
   *
   * Casting a column with a RowIndex is currently not supported.
   */
  virtual void cast_into(StringColumn<uint32_t>*) const;
  virtual void cast_into(StringColumn<uint64_t>*) const;
  virtual void cast_into(PyObjectColumn*) const;


  /**
   * Sets every row in the column to an NA value. As of now this method
   * modifies every element in the column's memory buffer regardless of its
   * refcount or rowindex. Use with caution.
   * This implementation will be made safer after Column::extract is modified
   * to be an in-place operation.
   */
  virtual void fill_na() = 0;

private:
  static Column* new_column(SType);
  static Column* from_py_iterable(const iterable*, int stype0);

  // FIXME
  friend FreadReader;  // friend Column* realloc_column(Column *col, SType stype, size_t nrows, int j);
};



//==============================================================================

template <typename T> class FwColumn : public Column
{
public:
  FwColumn(size_t nrows);
  FwColumn(size_t nrows, MemoryRange&&);
  void replace_buffer(MemoryRange&&) override;
  const T* elements_r() const;
  T* elements_w();
  T get_elem(size_t i) const;
  void set_elem(size_t i, T value);

  size_t data_nrows() const override;
  void resize_and_fill(size_t nrows) override;
  void apply_na_mask(const BoolColumn* mask) override;
  size_t elemsize() const override;
  bool is_fixedwidth() const override;
  virtual void reify() override;
  virtual void replace_values(RowIndex at, const Column* with) override;
  void replace_values(const RowIndex& at, T with);
  virtual RowIndex join(const Column* keycol) const override;
  void fill_na_mask(int8_t* outmask, size_t row0, size_t row1) override;

protected:
  void init_data() override;
  void init_mmap(const std::string& filename) override;
  void open_mmap(const std::string& filename, bool) override;
  void init_xbuf(Py_buffer* pybuffer) override;
  static constexpr T na_elem = GETNA<T>();
  void rbind_impl(std::vector<const Column*>& columns, size_t nrows,
                  bool isempty) override;
  void fill_na() override;

  FwColumn();
  friend Column;
};


template <> void FwColumn<PyObject*>::set_elem(size_t, PyObject*);
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
  using FwColumn<int8_t>::FwColumn;
  SType stype() const noexcept override;

  int8_t min() const;
  int8_t max() const;
  int8_t mode() const;
  int64_t sum() const;
  double mean() const;
  double sd() const;
  BooleanStats* get_stats() const override;

  py::oobj get_value_at_index(size_t i) const override;

  protected:

  void verify_integrity(const std::string& name) const override;

  using Column::mbuf;
  friend Column;
};



//==============================================================================

template <typename T> class IntColumn : public FwColumn<T>
{
public:
  using FwColumn<T>::FwColumn;
  virtual SType stype() const noexcept override;

  T min() const;
  T max() const;
  T mode() const;
  int64_t sum() const;
  double mean() const;
  double sd() const;
  double skew() const;
  double kurt() const;
  int64_t min_int64() const override;
  int64_t max_int64() const override;
  IntegerStats<T>* get_stats() const override;

  py::oobj get_value_at_index(size_t i) const override;

protected:
  void cast_into(StringColumn<uint32_t>*) const override;
  void cast_into(StringColumn<uint64_t>*) const override;

  using Column::stats;
  using Column::mbuf;
  using Column::new_data_column;
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
  using FwColumn<T>::FwColumn;
  virtual SType stype() const noexcept override;

  T min() const;
  T max() const;
  T mode() const;
  double sum() const;
  double mean() const;
  double sd() const;
  double skew() const;
  double kurt() const;
  RealStats<T>* get_stats() const override;

  py::oobj get_value_at_index(size_t i) const override;

protected:
  void cast_into(StringColumn<uint32_t>*) const override;
  void cast_into(StringColumn<uint64_t>*) const override;

  using Column::stats;
  using Column::new_data_column;
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
  PyObjectColumn(size_t nrows);
  PyObjectColumn(size_t nrows, MemoryRange&&);
  virtual SType stype() const noexcept override;
  PyObjectStats* get_stats() const override;

  py::oobj get_value_at_index(size_t i) const override;

protected:
  PyObjectColumn();
  // TODO: This should be corrected when PyObjectStats is implemented
  void open_mmap(const std::string& filename, bool) override;

  void cast_into(StringColumn<uint32_t>*) const override;
  void cast_into(StringColumn<uint64_t>*) const override;

  void replace_buffer(MemoryRange&&) override;
  void rbind_impl(std::vector<const Column*>& columns, size_t nrows,
                  bool isempty) override;

  void resize_and_fill(size_t nrows) override;
  void fill_na() override;
  void reify() override;
  friend Column;
};



//==============================================================================
// String column
//==============================================================================

template <typename T> class StringColumn : public Column
{
  MemoryRange strbuf;

public:
  StringColumn(size_t nrows);
  void save_to_disk(const std::string& filename,
                    WritableBuffer::Strategy strategy) override;
  void replace_buffer(MemoryRange&&, MemoryRange&&) override;

  SType stype() const noexcept override;
  size_t elemsize() const override;
  bool is_fixedwidth() const override;

  void reify() override;
  void resize_and_fill(size_t nrows) override;
  void apply_na_mask(const BoolColumn* mask) override;
  RowIndex join(const Column* keycol) const override;

  MemoryRange str_buf() { return strbuf; }
  size_t datasize() const;
  size_t data_nrows() const override;
  const char* strdata() const;
  const uint8_t* ustrdata() const;
  const T* offsets() const;
  T* offsets_w();

  CString mode() const;

  Column* shallowcopy(const RowIndex& new_rowindex) const override;
  void replace_values(RowIndex at, const Column* with) override;
  StringStats<T>* get_stats() const override;

  void verify_integrity(const std::string& name) const override;

  py::oobj get_value_at_index(size_t i) const override;
  void fill_na_mask(int8_t* outmask, size_t row0, size_t row1) override;

protected:
  StringColumn();
  StringColumn(size_t nrows, MemoryRange&& offbuf, MemoryRange&& strbuf);
  void init_data() override;
  void init_mmap(const std::string& filename) override;
  void open_mmap(const std::string& filename, bool recode) override;
  void init_xbuf(Py_buffer* pybuffer) override;

  void rbind_impl(std::vector<const Column*>& columns, size_t nrows,
                  bool isempty) override;

  void cast_into(PyObjectColumn*) const override;
  void cast_into(StringColumn<uint64_t>*) const override;
  void fill_na() override;

  friend Column;
  friend FreadReader;  // friend Column* alloc_column(SType, size_t, int);
  friend Column* new_string_column(size_t, MemoryRange&&, MemoryRange&&);
};


/**
 * Use this function to create a column from existing offsets & strdata.
 * It will create either StringColumn<uint32_t>* or StringColumn<uint64_t>*
 * depending on the size of the data.
 */
Column* new_string_column(size_t n, MemoryRange&& data, MemoryRange&& str);


template <> void StringColumn<uint32_t>::cast_into(StringColumn<uint64_t>*) const;
template <> void StringColumn<uint64_t>::cast_into(StringColumn<uint64_t>*) const;
extern template class StringColumn<uint32_t>;
extern template class StringColumn<uint64_t>;



//==============================================================================

// "Fake" column, its only use is to serve as a placeholder for a Column with an
// unknown type. This column cannot be put into a DataTable.
class VoidColumn : public Column {
  public:
    VoidColumn(size_t nrows);
    SType stype() const noexcept override;
    size_t elemsize() const override;
    bool is_fixedwidth() const override;
    size_t data_nrows() const override;
    void reify() override;
    void resize_and_fill(size_t) override;
    void rbind_impl(std::vector<const Column*>&, size_t, bool) override;
    void apply_na_mask(const BoolColumn*) override;
    void replace_values(RowIndex, const Column*) override;
    RowIndex join(const Column* keycol) const override;
    Stats* get_stats() const override;
    py::oobj get_value_at_index(size_t i) const override;
    void fill_na_mask(int8_t* outmask, size_t row0, size_t row1) override;
  protected:
    VoidColumn();
    void init_data() override;
    void init_mmap(const std::string&) override;
    void open_mmap(const std::string&, bool) override;
    void init_xbuf(Py_buffer*) override;
    void fill_na() override;

    friend Column;
};




#endif
