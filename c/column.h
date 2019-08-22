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
#ifndef dt_COLUMN_h
#define dt_COLUMN_h
#include <string>
#include <vector>
#include "python/list.h"
#include "python/obj.h"
#include "groupby.h"
#include "memrange.h"     // MemoryRange
#include "rowindex.h"
#include "stats.h"
#include "types.h"

class ColumnImpl;
class DataTable;
class BoolColumn;
class PyObjectColumn;
class FreadReader;  // used as a friend
template <typename T> class IntColumn;
template <typename T> class RealColumn;
template <typename T> class StringColumn;

using colvec = std::vector<OColumn>;
using strvec = std::vector<std::string>;

/**
 * Helper template to convert between an stype and the C type
 * of the underlying column element:
 *
 * element_t<stype>
 *   resolves to the type of the element that is in the main data buffer
 *   of `column_t<stype>`.
 */
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



//------------------------------------------------------------------------------
// OColumn
//------------------------------------------------------------------------------

// "owner" of a ColumnImpl
// Eventually will be renamed into simple ColumnImpl
class OColumn
{
  private:
    // shared pointer; its lifetime is governed by `pcol->refcount`: when the
    // reference count reaches 0, the object is deleted.
    // We do not use std::shared_ptr<> here primarily because it makes it much
    // harder to inspect the object in GDB / LLDB.
    ColumnImpl* pcol;

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
    explicit OColumn(ColumnImpl* col);

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

    ColumnImpl* operator->();
    const ColumnImpl* operator->() const;


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
  // ColumnImpl manipulation
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
    friend class ColumnImpl;
};


/**
 * Use this function to create a column from existing offsets & strdata.
 * It will create either StringColumn<uint32_t>* or StringColumn<uint64_t>*
 * depending on the size of the data.
 */
OColumn new_string_column(size_t n, MemoryRange&& data, MemoryRange&& str);


#endif
