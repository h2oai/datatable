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
class Column {
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
  Column(const Column&) = delete;
  Column(Column&&) = delete;
  virtual ~Column();

  virtual SType stype() const;
  inline void* data() const { return mbuf->get(); }
  inline void* data_at(size_t i) const { return mbuf->at(i); }
  inline RowIndex* rowindex() const { return ri; }
  size_t alloc_size() const;
  virtual int64_t data_nrows() const;
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
  virtual void resize_and_fill(int64_t nrows);

  /**
   * Create a shallow copy of this Column, possibly applying the provided
   * RowIndex. The copy is "shallow" in the sense that the main data buffer
   * is copied by-reference. If this column has a rowindex, and the user
   * asks to apply a new rowindex, then the new one will replace the original.
   * If you want the rowindices to be merged, you should merge them manually
   * and pass the merged rowindex to this method.
   */
  Column* shallowcopy(RowIndex* new_rowindex = nullptr);

  Column* deepcopy() const;
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
                          int64_t nrows, bool isempty);

private:
  static size_t allocsize0(SType, int64_t nrows);
  static Column* new_column(SType);
  // FIXME
  friend Column* try_to_resolve_object_column(Column* col);
  friend Column* column_from_list(PyObject *list);
  friend Column* realloc_column(Column *col, SType stype, size_t nrows, int j);
  friend void setFinalNrow(size_t nrows);
};



//==============================================================================

template <typename T> class FwColumn : public Column
{
public:
  FwColumn();
  FwColumn(int64_t nrows);
  T* elements();
  T get_elem(int64_t i) const;
  void set_elem(int64_t i, T value);

  int64_t data_nrows() const override;
  void resize_and_fill(int64_t nrows) override;

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
  virtual SType stype() const override;
};



//==============================================================================

template <typename T> class IntColumn : public FwColumn<T>
{
public:
  using FwColumn<T>::FwColumn;
  virtual ~IntColumn();
  virtual SType stype() const override;
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
  virtual ~RealColumn();
  virtual SType stype() const override;
};

extern template class RealColumn<float>;
extern template class RealColumn<double>;



//==============================================================================

class PyObjectColumn : public FwColumn<PyObject*>
{
public:
  using FwColumn<PyObject*>::FwColumn;
  virtual ~PyObjectColumn();
  virtual SType stype() const override;
};



//==============================================================================

template <typename T> class StringColumn : public Column
{
  MemoryBuffer *strbuf;

public:
  StringColumn();
  StringColumn(int64_t nrows);
  virtual ~StringColumn();
  SType stype() const override;
  Column* extract_simple_slice(RowIndex*) const;
  void resize_and_fill(int64_t nrows) override;

  size_t datasize();
  int64_t data_nrows() const override;
  static size_t padding(size_t datasize);

protected:
  void rbind_impl(const std::vector<const Column*>& columns, int64_t nrows,
                  bool isempty) override;
};


extern template class StringColumn<int32_t>;
extern template class StringColumn<int64_t>;



//==============================================================================

class VoidColumn : public Column {
public:
  VoidColumn(int64_t nrows = 0) : Column(nrows) {}
  SType stype() const override { return ST_VOID; }
};



//==============================================================================
typedef Column* (*castfn_ptr)(const Column*, Column*);

Column* column_from_list(PyObject*);
void init_column_cast_functions(void);
// Implemented in py_column_cast.c
void init_column_cast_functions2(castfn_ptr hardcasts[][DT_STYPES_COUNT]);

#endif
