//------------------------------------------------------------------------------
//  Copyright 2017 H2O.ai
//
//  Licensed under the Apache License, Version 2.0 (the "License");
//  you may not use this file except in compliance with the License.
//  You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
//  Unless required by applicable law or agreed to in writing, software
//  distributed under the License is distributed on an "AS IS" BASIS,
//  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//  See the License for the specific language governing permissions and
//  limitations under the License.
//------------------------------------------------------------------------------
#ifndef dt_COLUMN_H
#define dt_COLUMN_H
#include <Python.h>
#include <stdint.h>
#include <vector>
#include "memorybuf.h"
#include "types.h"
#include "stats.h"

class DataTable;
class RowIndex;
class Stats;
class BoolColumn;
class PyObjectColumn;
template <typename T> class IntColumn;
template <typename T> class RealColumn;
template <typename T> class StringColumn;


//==============================================================================

/**
 * This class represents a single column within a DataTable.
 *
 *
 * Parameters
 * ----------
 * data
 *     Raw data buffer in NFF format, depending on the column's `stype` (see
 *     specification in "types.h"). This may be NULL if column has 0 rows.
 *
 * stype
 *     Type of data within the column (the enum is defined in types.h). This is
 *     the "storage" type, i.e. how the data is actually stored in memory. The
 *     logical type can be derived from `stype` via `stype_info[stype].ltype`.
 *
 * nrows
 *     Number of elements in this column. For fixed-size stypes, this should be
 *     equal to `alloc_size / stype_info[stype].elemsize`.
 *
 * meta
 *     Metadata associated with the column, if any, otherwise NULL. This is one
 *     of the *Meta structs defined in "types.h". The actual struct used
 *     depends on the `stype`.
 *
 */
class Column
{
protected:
  MemoryBuffer *mbuf;
  RowIndex *ri;

public:  // TODO: convert these into private
  void   *meta;        // 8
  int64_t nrows;       // 8
  Stats*  stats;       // 8

public:
  static Column* new_data_column(SType, int64_t nrows);
  static Column* new_mmap_column(SType, int64_t nrows, const char* filename);
  static Column* open_mmap_column(SType, int64_t nrows, const char* filename,
                                  const char* metastr);
  static Column* new_xbuf_column(SType, int64_t nrows, void* pybuffer,
                                 void* data, size_t datasize);
  static Column* from_pylist(PyObject* list, int stype0 = 0, int ltype0 = 0);

  Column(const Column&) = delete;
  Column(Column&&) = delete;
  virtual ~Column();
  virtual void replace_buffer(MemoryBuffer*, MemoryBuffer*) = 0;

  virtual SType stype() const = 0;
  inline void* data() const { return mbuf->get(); }
  inline void* data_at(size_t i) const { return mbuf->at(i); }
  inline RowIndex* rowindex() const { return ri; }
  size_t alloc_size() const;
  virtual int64_t data_nrows() const = 0;
  PyObject* mbuf_repr() const;
  int mbuf_refcount() const;
  size_t memory_footprint() const;

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
  Column* shallowcopy(RowIndex* new_rowindex = nullptr) const;

  Column* deepcopy() const;

  /**
   * Factory method to cast the current column into the given `stype`. If a
   * column is cast into its own stype, a shallow copy is returned. Otherwise,
   * this method constructs a new column of the provided stype and writes the
   * converted data into it.
   */
  Column* cast(SType) const;

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
  Column* rbind(const std::vector<const Column*>& columns);

  Column* extract();
  Column* save_to_disk(const char*);
  RowIndex* sort() const;

  static size_t i4s_padding(size_t datasize);
  static size_t i8s_padding(size_t datasize);

protected:
  Column(int64_t nrows);
  virtual void rbind_impl(const std::vector<const Column*>& columns,
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

private:
  static size_t allocsize0(SType, int64_t nrows);
  static Column* new_column(SType);
  // FIXME
  friend Column* try_to_resolve_object_column(Column* col);
  friend Column* realloc_column(Column *col, SType stype, size_t nrows, int j);
  friend void setFinalNrow(size_t nrows);
};



//==============================================================================

template <typename T> class FwColumn : public Column
{
public:
  FwColumn();
  FwColumn(int64_t nrows);
  void replace_buffer(MemoryBuffer*, MemoryBuffer*) override;

  T* elements() const;
  T get_elem(int64_t i) const;
  void set_elem(int64_t i, T value);

  int64_t data_nrows() const override;
  void resize_and_fill(int64_t nrows) override;
  void apply_na_mask(const BoolColumn* mask) override;

protected:
  static constexpr T na_elem = GETNA<T>();
  Column* extract_simple_slice(RowIndex*) const;
  void rbind_impl(const std::vector<const Column*>& columns, int64_t nrows,
                  bool isempty) override;
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
  virtual ~BoolColumn();
  SType stype() const override;

protected:
  void cast_into(BoolColumn*) const override;
  void cast_into(IntColumn<int8_t>*) const override;
  void cast_into(IntColumn<int16_t>*) const override;
  void cast_into(IntColumn<int32_t>*) const override;
  void cast_into(IntColumn<int64_t>*) const override;
  void cast_into(RealColumn<float>*) const override;
  void cast_into(RealColumn<double>*) const override;
  void cast_into(PyObjectColumn*) const override;
  // void cast_into(StringColumn<int32_t>*) const;
  // void cast_into(StringColumn<int64_t>*) const;
};



//==============================================================================

template <typename T> class IntColumn : public FwColumn<T>
{
public:
  using FwColumn<T>::FwColumn;
  virtual ~IntColumn();
  virtual SType stype() const override;

protected:
  void cast_into(BoolColumn*) const override;
  void cast_into(IntColumn<int8_t>*) const override;
  void cast_into(IntColumn<int16_t>*) const override;
  void cast_into(IntColumn<int32_t>*) const override;
  void cast_into(IntColumn<int64_t>*) const override;
  void cast_into(RealColumn<float>*) const override;
  void cast_into(RealColumn<double>*) const override;
  void cast_into(PyObjectColumn*) const override;
  // void cast_into(StringColumn<int32_t>*) const;
  // void cast_into(StringColumn<int64_t>*) const;
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
  virtual ~RealColumn();
  virtual SType stype() const override;

protected:
  void cast_into(BoolColumn*) const override;
  void cast_into(IntColumn<int8_t>*) const override;
  void cast_into(IntColumn<int16_t>*) const override;
  void cast_into(IntColumn<int32_t>*) const override;
  void cast_into(IntColumn<int64_t>*) const override;
  void cast_into(RealColumn<float>*) const override;
  void cast_into(RealColumn<double>*) const override;
  void cast_into(PyObjectColumn*) const override;
  // void cast_into(StringColumn<int32_t>*) const;
  // void cast_into(StringColumn<int64_t>*) const;
};

template <> void RealColumn<float>::cast_into(RealColumn<float>*) const;
template <> void RealColumn<float>::cast_into(RealColumn<double>*) const;
template <> void RealColumn<double>::cast_into(RealColumn<float>*) const;
template <> void RealColumn<double>::cast_into(RealColumn<double>*) const;
extern template class RealColumn<float>;
extern template class RealColumn<double>;



//==============================================================================

class PyObjectColumn : public FwColumn<PyObject*>
{
public:
  using FwColumn<PyObject*>::FwColumn;
  virtual ~PyObjectColumn();
  virtual SType stype() const override;

protected:
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
};



//==============================================================================

template <typename T> class StringColumn : public Column
{
  MemoryBuffer *strbuf;

public:
  StringColumn();
  StringColumn(int64_t nrows);
  virtual ~StringColumn();
  void replace_buffer(MemoryBuffer*, MemoryBuffer*) override;

  SType stype() const override;
  Column* extract_simple_slice(RowIndex*) const;
  void resize_and_fill(int64_t nrows) override;
  void apply_na_mask(const BoolColumn* mask) override;

  size_t datasize();
  int64_t data_nrows() const override;
  static size_t padding(size_t datasize);
  char* strdata() const;
  T* offsets() const;

protected:
  void rbind_impl(const std::vector<const Column*>& columns, int64_t nrows,
                  bool isempty) override;

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
};


extern template class StringColumn<int32_t>;
extern template class StringColumn<int64_t>;



//==============================================================================

// "Fake" column, its only use is to serve as a placeholder for a Column with an
// unknown type. This column cannot be put into a DataTable.
class VoidColumn : public Column {
public:
  VoidColumn(int64_t nrows = 0) : Column(nrows) {}
  void replace_buffer(MemoryBuffer*, MemoryBuffer*) override {}
  SType stype() const override { return ST_VOID; }
  int64_t data_nrows() const override { return nrows; }
  void resize_and_fill(int64_t) override {}
  void rbind_impl(const std::vector<const Column*>&, int64_t, bool) override {}
  void apply_na_mask(const BoolColumn*) override {}
};




#endif
