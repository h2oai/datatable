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
#include "memorybuf.h"
#include "types.h"
#include "stats.h"

class DataTable;
class RowIndex;
class Stats;


//==============================================================================

/**
 * Single "data" column within a datatable.
 *
 * The `data` memory buffer is the raw storage of the column's data. The data
 * is stored in the "NFF" format, as described in the "types.h" file. Typically
 * it's just a plain array of primitive types, in which case the `j`-th element
 * can be accessed as `((ctype*)(column->data))[j]`. More complicated types
 * (such as strings) require a more elaborate access scheme.
 *
 * Parameters
 * ----------
 * data
 *     Raw data buffer in NFF format, depending on the column's `stype` (see
 *     specification in "types.h"). This may be NULL if column has 0 rows.
 *
 * mtype
 *     "Memory" type of the column -- i.e. where the data is actually stored.
 *     Possible values are MT_DATA (column is stored in RAM), MT_MMAP (memory-
 *     -mapped from disk), and MT_XBUF (acquired from an external buffer).
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
 * alloc_size
 *     Size (in bytes) of memory buffer `data`. For columns of type MT_MMAP this
 *     size is equal to the file size.
 *
 * refcount
 *     Number of references to this Column object. It is a common for a single
 *     Column to be reused across multiple DataTables (for example when a new
 *     DataTable is created as a view onto an existing one). In such case the
 *     refcount of the Column should be increased. Any modification to a Column
 *     with refcount > 1 is not allowed -- a copy should be made first. When a
 *     DataTable is deleted, it decreases refcounts of all its constituent
 *     Columns, and once a Column's refcount becomes 0 it can be safely deleted.
 */
class Column
{
protected:
  MemoryBuffer *mbuf;

public:  // TODO: convert these into private
  void   *meta;        // 8
  int64_t nrows;       // 8
  Stats*  stats;       // 8

private:
  SType   _stype;      // 1
  int64_t : 56;        // padding

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
  void* data() const;
  void* data_at(size_t) const;
  size_t alloc_size() const;
  PyObject* mbuf_repr() const;
  int mbuf_refcount() const;

  /**
   * Resize the column up to `nrows` elements, and fill all new elements with
   * NA values.
   */
  void resize_and_fill(int64_t nrows);

  Column* shallowcopy();
  Column* deepcopy();
  Column* cast(SType);
  Column* rbind(Column**);
  Column* extract(RowIndex* = NULL);
  Column* save_to_disk(const char*);
  size_t i4s_datasize();
  size_t i8s_datasize();
  size_t get_allocsize();

  static RowIndex* sort(Column*, RowIndex*);
  static size_t i4s_padding(size_t datasize);
  static size_t i8s_padding(size_t datasize);

protected:
  static size_t allocsize0(SType, int64_t nrows);
  Column(int64_t nrows);
  Column(SType stype, int64_t nrows);
  Column* rbind_fw(Column**, int64_t, int);  // helper for rbind
  Column* rbind_str32(Column**, int64_t, int);

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
  FwColumn(int64_t nrows);
  T* elements();
  T get_elem(int64_t i) const;
  void set_elem(int64_t i, T value);

protected:
  // static constexpr size_t elemsize = sizeof(T);
  // static constexpr T na_elem = GETNA<T>();
  virtual Column* extract_simple_slice(RowIndex*) const;
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

template <typename T> class StringColumn : public Column
{
  MemoryBuffer *strbuf;

public:
  // static const size_t elemsize = sizeof(T);

  StringColumn(int64_t nrows);
  virtual ~StringColumn();
  virtual Column* extract_simple_slice(RowIndex*) const;
  virtual SType stype() const override;

  static size_t padding(size_t datasize);
};



//==============================================================================

class PyObjectColumn : public FwColumn<PyObject*>
{
public:
  using FwColumn<PyObject*>::FwColumn;
  virtual ~PyObjectColumn();
  virtual SType stype() const override;
};



//==============================================================================
typedef Column* (*castfn_ptr)(Column*, Column*);

Column* column_from_list(PyObject*);
void init_column_cast_functions(void);
// Implemented in py_column_cast.c
void init_column_cast_functions2(castfn_ptr hardcasts[][DT_STYPES_COUNT]);

#endif
