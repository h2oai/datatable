//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#ifndef dt_COLUMN_h
#define dt_COLUMN_h
#include <string>
#include <vector>
#include <Python.h>
#include "memorybuf.h"
#include "py_types.h"
#include "python/list.h"
#include "rowindex.h"
#include "stats.h"
#include "types.h"

class DataTable;
class BoolColumn;
class PyObjectColumn;
class FreadReader;  // used as a friend
template <typename T> class IntColumn;
template <typename T> class RealColumn;
template <typename T> class StringColumn;


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
  int64_t nrows;

public:
  static Column* new_data_column(SType, int64_t nrows);
  static Column* new_na_column(SType, int64_t nrows);
  static Column* new_mmap_column(SType, int64_t nrows, const std::string& filename);
  static Column* open_mmap_column(SType, int64_t nrows, const std::string& filename);
  static Column* new_xbuf_column(SType, int64_t nrows, Py_buffer* pybuffer);
  static Column* new_mbuf_column(SType, MemoryRange&&);
  static Column* new_mbuf_column(SType, MemoryRange&&, MemoryRange&&);
  static Column* from_pylist(PyyList& list, int stype0 = 0, int ltype0 = 0);

  Column(const Column&) = delete;
  Column(Column&&) = delete;
  virtual ~Column();
  virtual void replace_buffer(MemoryRange&&);
  virtual void replace_buffer(MemoryRange&&, MemoryRange&&);

  virtual SType stype() const = 0;
  virtual size_t elemsize() const = 0;
  virtual bool is_fixedwidth() const = 0;

  void replace_rowindex(const RowIndex& newri);

  MemoryRange data_buf() const { return mbuf; }
  const void* data() const { return mbuf.rptr(); }
  void* data_w() { return mbuf.wptr(); }
  PyObject* mbuf_repr() const;
  size_t alloc_size() const;

  const RowIndex& rowindex() const { return ri; }
  virtual int64_t data_nrows() const = 0;
  size_t memory_footprint() const;
  RowIndex sort(bool make_groups) const;

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
  virtual void resize_and_fill(int64_t nrows) = 0;

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
   * Individual entries in the `columns` array may be instances of `VoidColumn,
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

  virtual void save_to_disk(const std::string&, WritableBuffer::Strategy);

  int64_t countna() const;
  int64_t nunique() const;
  int64_t nmodal() const;
  virtual int64_t min_int64() const { return GETNA<int64_t>(); }
  virtual int64_t max_int64() const { return GETNA<int64_t>(); }

  /**
   * Methods for retrieving statistics in the form of a Column. The resulting
   * Column will contain a single row, in which is the value of the statistic.
   * Fixed-type statistics (e.g mean, countna) will always return a Column with
   * a corresponding stype, even if the statistic results in a NA value. For
   * example, `mean_column` will always return RealColumn<double>.
   * Variable-type statistics (e.g. min, sum) will instead result in a column of
   * the same stype as the calling instance if the statistic is incompatible
   * with the column stype.
   */
  virtual Column* min_column() const;
  virtual Column* max_column() const;
  virtual Column* sum_column() const;
  virtual Column* mean_column() const;
  virtual Column* sd_column() const;
  virtual Column* countna_column() const;
  virtual Column* nunique_column() const;
  virtual Column* nmodal_column() const;
  virtual Column* mode_column() const;

  virtual PyObject* min_pyscalar() const;
  virtual PyObject* max_pyscalar() const;
  virtual PyObject* sum_pyscalar() const;
  virtual PyObject* mean_pyscalar() const;
  virtual PyObject* sd_pyscalar() const;
  virtual PyObject* countna_pyscalar() const;
  virtual PyObject* nunique_pyscalar() const;
  virtual PyObject* nmodal_pyscalar() const;
  virtual PyObject* mode_pyscalar() const;

  /**
   * Check that the data in this Column object is correct. Use the provided
   * `IntegrityCheckContext` to report any errors, and `name` is the name of
   * the column to be used in the output messages.
   */
  virtual bool verify_integrity(IntegrityCheckContext&,
                                const std::string& name = "Column") const;

protected:
  Column(int64_t nrows = 0);
  virtual void init_data() = 0;
  virtual void init_mmap(const std::string& filename) = 0;
  virtual void open_mmap(const std::string& filename) = 0;
  virtual void init_xbuf(Py_buffer* pybuffer) = 0;
  virtual void rbind_impl(std::vector<const Column*>& columns,
                          int64_t nrows, bool isempty) = 0;

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
  virtual void cast_into(BoolColumn*) const;
  virtual void cast_into(IntColumn<int8_t>*) const;
  virtual void cast_into(IntColumn<int16_t>*) const;
  virtual void cast_into(IntColumn<int32_t>*) const;
  virtual void cast_into(IntColumn<int64_t>*) const;
  virtual void cast_into(RealColumn<float>*) const;
  virtual void cast_into(RealColumn<double>*) const;
  virtual void cast_into(StringColumn<int32_t>*) const;
  virtual void cast_into(StringColumn<int64_t>*) const;
  virtual void cast_into(PyObjectColumn*) const;


  /**
   * Sets every row in the column to an NA value. As of now this method
   * modifies every element in the column's memory buffer regardless of its
   * refcount or rowindex. Use with caution.
   * This implementation will be made safer after Column::extract is modified
   * to be an in-place operation.
   */
  virtual void fill_na() = 0;

  /**
   * Getter method for this column's reference to `Stats`. If the reference
   * is null then the method will create a new Stats instance for this column
   * and return a pointer to said instance.
   */
  virtual Stats* get_stats() const = 0;

private:
  static Column* new_column(SType);

  // FIXME
  friend FreadReader;  // friend Column* realloc_column(Column *col, SType stype, size_t nrows, int j);
};



//==============================================================================

template <typename T> class FwColumn : public Column
{
public:
  FwColumn(int64_t nrows);
  FwColumn(int64_t nrows, MemoryRange&&);
  void replace_buffer(MemoryRange&&) override;
  const T* elements_r() const;
  T* elements_w();
  T get_elem(int64_t i) const;
  void set_elem(int64_t i, T value);

  int64_t data_nrows() const override;
  void resize_and_fill(int64_t nrows) override;
  void apply_na_mask(const BoolColumn* mask) override;
  size_t elemsize() const override;
  bool is_fixedwidth() const override;
  virtual void reify() override;
  virtual void replace_values(RowIndex at, const Column* with) override;

protected:
  void init_data() override;
  void init_mmap(const std::string& filename) override;
  void open_mmap(const std::string& filename) override;
  void init_xbuf(Py_buffer* pybuffer) override;
  static constexpr T na_elem = GETNA<T>();
  void rbind_impl(std::vector<const Column*>& columns, int64_t nrows,
                  bool isempty) override;
  void fill_na() override;

  FwColumn();
  friend Column;
};


template <> void FwColumn<PyObject*>::set_elem(int64_t, PyObject*);
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
  SType stype() const override;

  int8_t min() const;
  int8_t max() const;
  int8_t mode() const;
  int64_t sum() const;
  double mean() const;
  double sd() const;

  Column* min_column() const override;
  Column* max_column() const override;
  Column* mode_column() const override;
  Column* sum_column() const override;
  Column* mean_column() const override;
  Column* sd_column() const override;
  PyObject* min_pyscalar() const override;
  PyObject* max_pyscalar() const override;
  PyObject* mode_pyscalar() const override;
  PyObject* sum_pyscalar() const override;
  PyObject* mean_pyscalar() const override;
  PyObject* sd_pyscalar() const override;

protected:
  BooleanStats* get_stats() const override;

  void cast_into(BoolColumn*) const override;
  void cast_into(IntColumn<int8_t>*) const override;
  void cast_into(IntColumn<int16_t>*) const override;
  void cast_into(IntColumn<int32_t>*) const override;
  void cast_into(IntColumn<int64_t>*) const override;
  void cast_into(RealColumn<float>*) const override;
  void cast_into(RealColumn<double>*) const override;
  void cast_into(PyObjectColumn*) const override;
  void cast_into(StringColumn<int32_t>*) const override;
  void cast_into(StringColumn<int64_t>*) const override;

  bool verify_integrity(IntegrityCheckContext&,
                        const std::string& name = "Column") const override;

  using Column::mbuf;
  friend Column;
};



//==============================================================================

template <typename T> class IntColumn : public FwColumn<T>
{
public:
  using FwColumn<T>::FwColumn;
  virtual SType stype() const override;

  T min() const;
  T max() const;
  T mode() const;
  int64_t sum() const;
  double mean() const;
  double sd() const;
  int64_t min_int64() const override;
  int64_t max_int64() const override;

  Column* min_column() const override;
  Column* max_column() const override;
  Column* mode_column() const override;
  Column* sum_column() const override;
  Column* mean_column() const override;
  Column* sd_column() const override;
  PyObject* min_pyscalar() const override;
  PyObject* max_pyscalar() const override;
  PyObject* mode_pyscalar() const override;
  PyObject* sum_pyscalar() const override;
  PyObject* mean_pyscalar() const override;
  PyObject* sd_pyscalar() const override;

protected:
  IntegerStats<T>* get_stats() const override;

  void cast_into(BoolColumn*) const override;
  void cast_into(IntColumn<int8_t>*) const override;
  void cast_into(IntColumn<int16_t>*) const override;
  void cast_into(IntColumn<int32_t>*) const override;
  void cast_into(IntColumn<int64_t>*) const override;
  void cast_into(RealColumn<float>*) const override;
  void cast_into(RealColumn<double>*) const override;
  void cast_into(PyObjectColumn*) const override;
  void cast_into(StringColumn<int32_t>*) const override;
  void cast_into(StringColumn<int64_t>*) const override;

  using Column::stats;
  using Column::mbuf;
  using Column::new_data_column;
  friend Column;
};

template <> void IntColumn<int8_t>::cast_into(IntColumn<int8_t>*) const;
template <> void IntColumn<int16_t>::cast_into(IntColumn<int16_t>*) const;
template <> void IntColumn<int32_t>::cast_into(IntColumn<int32_t>*) const;
template <> void IntColumn<int64_t>::cast_into(IntColumn<int64_t>*) const;
extern template class IntColumn<int8_t>;
extern template class IntColumn<int16_t>;
extern template class IntColumn<int32_t>;
extern template class IntColumn<int64_t>;


//==============================================================================

template <typename T> class RealColumn : public FwColumn<T>
{
public:
  using FwColumn<T>::FwColumn;
  virtual SType stype() const override;

  T min() const;
  T max() const;
  T mode() const;
  double sum() const;
  double mean() const;
  double sd() const;

  Column* min_column() const override;
  Column* max_column() const override;
  Column* mode_column() const override;
  Column* sum_column() const override;
  Column* mean_column() const override;
  Column* sd_column() const override;
  PyObject* min_pyscalar() const override;
  PyObject* max_pyscalar() const override;
  PyObject* mode_pyscalar() const override;
  PyObject* sum_pyscalar() const override;
  PyObject* mean_pyscalar() const override;
  PyObject* sd_pyscalar() const override;

protected:
  RealStats<T>* get_stats() const override;

  void cast_into(BoolColumn*) const override;
  void cast_into(IntColumn<int8_t>*) const override;
  void cast_into(IntColumn<int16_t>*) const override;
  void cast_into(IntColumn<int32_t>*) const override;
  void cast_into(IntColumn<int64_t>*) const override;
  void cast_into(RealColumn<float>*) const override;
  void cast_into(RealColumn<double>*) const override;
  void cast_into(PyObjectColumn*) const override;
  void cast_into(StringColumn<int32_t>*) const override;
  void cast_into(StringColumn<int64_t>*) const override;

  using Column::stats;
  using Column::new_data_column;
  friend Column;
};

template <> void RealColumn<float>::cast_into(RealColumn<float>*) const;
template <> void RealColumn<float>::cast_into(RealColumn<double>*) const;
template <> void RealColumn<double>::cast_into(RealColumn<float>*) const;
template <> void RealColumn<double>::cast_into(RealColumn<double>*) const;
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
  PyObjectColumn(int64_t nrows);
  PyObjectColumn(int64_t nrows, MemoryRange&&);
  virtual SType stype() const override;

protected:
  PyObjectColumn();
  // TODO: This should be corrected when PyObjectStats is implemented
  PyObjectStats* get_stats() const override;
  void open_mmap(const std::string& filename) override;

  // void cast_into(BoolColumn*) const override;
  // void cast_into(IntColumn<int8_t>*) const override;
  // void cast_into(IntColumn<int16_t>*) const override;
  // void cast_into(IntColumn<int32_t>*) const override;
  // void cast_into(IntColumn<int64_t>*) const override;
  // void cast_into(RealColumn<float>*) const override;
  // void cast_into(RealColumn<double>*) const override;
  void cast_into(PyObjectColumn*) const override;
  // void cast_into(StringColumn<int32_t>*) const;
  // void cast_into(StringColumn<int64_t>*) const;

  void replace_buffer(MemoryRange&&) override;
  void rbind_impl(std::vector<const Column*>& columns, int64_t nrows,
                  bool isempty) override;

  void resize_and_fill(int64_t nrows) override;
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
  StringColumn(int64_t nrows);
  StringColumn(int64_t nrows, MemoryRange&& offbuf, MemoryRange&& strbuf);
  void save_to_disk(const std::string& filename,
                    WritableBuffer::Strategy strategy) override;
  void replace_buffer(MemoryRange&&, MemoryRange&&) override;

  SType stype() const override;
  size_t elemsize() const override;
  bool is_fixedwidth() const override;

  void reify() override;
  void resize_and_fill(int64_t nrows) override;
  void apply_na_mask(const BoolColumn* mask) override;

  size_t datasize() const;
  int64_t data_nrows() const override;
  const char* strdata() const;
  const T* offsets() const;
  T* offsets_w();

  CString mode() const;
  PyObject* mode_pyscalar() const override;
  Column* mode_column() const override;

  Column* shallowcopy(const RowIndex& new_rowindex) const override;
  void replace_values(RowIndex at, const Column* with) override;

  bool verify_integrity(IntegrityCheckContext&,
                        const std::string& name = "Column") const override;

protected:
  StringColumn();
  void init_data() override;
  void init_mmap(const std::string& filename) override;
  void open_mmap(const std::string& filename) override;
  void init_xbuf(Py_buffer* pybuffer) override;

  void rbind_impl(std::vector<const Column*>& columns, int64_t nrows,
                  bool isempty) override;

  StringStats<T>* get_stats() const override;

  // void cast_into(BoolColumn*) const override;
  // void cast_into(IntColumn<int8_t>*) const override;
  // void cast_into(IntColumn<int16_t>*) const override;
  // void cast_into(IntColumn<int32_t>*) const override;
  // void cast_into(IntColumn<int64_t>*) const override;
  // void cast_into(RealColumn<float>*) const override;
  // void cast_into(RealColumn<double>*) const override;
  void cast_into(PyObjectColumn*) const override;
  // void cast_into(StringColumn<int32_t>*) const;
  void cast_into(StringColumn<int64_t>*) const override;
  void fill_na() override;

  //int verify_meta_integrity(std::vector<char>*, int, const char* = "Column") const override;

  friend Column;
  friend FreadReader;  // friend Column* alloc_column(SType, size_t, int);
};


template <> void StringColumn<int32_t>::cast_into(StringColumn<int64_t>*) const;
template <> void StringColumn<int64_t>::cast_into(StringColumn<int64_t>*) const;
extern template class StringColumn<int32_t>;
extern template class StringColumn<int64_t>;



//==============================================================================

// "Fake" column, its only use is to serve as a placeholder for a Column with an
// unknown type. This column cannot be put into a DataTable.
class VoidColumn : public Column
{
public:
  VoidColumn(int64_t nrows) : Column(nrows) {}
  SType stype() const override { return ST_VOID; }
  size_t elemsize() const override { return 0; }
  bool is_fixedwidth() const override { return true; }
  int64_t data_nrows() const override { return nrows; }
  void reify() override {}
  void resize_and_fill(int64_t) override {}
  void rbind_impl(std::vector<const Column*>&, int64_t, bool) override {}
  void apply_na_mask(const BoolColumn*) override {}
  void replace_values(RowIndex, const Column*) override {}
protected:
  VoidColumn() {}
  void init_data() override {}
  void init_mmap(const std::string&) override {}
  void open_mmap(const std::string&) override {}
  void init_xbuf(Py_buffer*) override {}

  Stats* get_stats() const override { return nullptr; }
  void fill_na() override {}

  friend Column;
};




#endif
