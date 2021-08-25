//------------------------------------------------------------------------------
// Copyright 2018-2021 H2O.ai
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
#include "_dt.h"
#include "stats.h"       // Stat (enum), Stats
#include "types/type.h"

namespace dt {
  class ColumnImpl;
}

enum class NaStorage : uint8_t {
  NONE,
  SENTINEL,
  BITMASK,
  VIRTUAL,
};




//------------------------------------------------------------------------------
// Column
//------------------------------------------------------------------------------

/**
 * A single column within a DataTable.
 *
 * A Column is a self-sufficient object, i.e. it may exist outside
 * of a DataTable too. This usually happens when a DataTable is being
 * transformed, then new Column objects will be created, manipulated,
 * and eventually bundled into a new DataTable.
 *
 * The class implements the Pimpl idiom: its implementation details
 * are hidden as a pointer `impl_` to a ColumnImpl instance. This
 * object is polymorphic, uses shared-ptr-like mechanics, and may be
 * replaced with some other instance on the fly.
 */
class Column
{
  private:
    // shared pointer; its lifetime is governed by `impl_->refcount`: when the
    // reference count reaches 0, the object is deleted.
    // We do not use std::shared_ptr<> here primarily because it makes it
    // harder to inspect the object in GDB / LLDB.
    //
    // This pointer is marked `const` in order to prevent accidental
    // modification of the shared data. Specifically, `impl_` is
    // potentially shared among multiple `Column` objects; and
    // modifying one Column should not affect any other instance.
    // Therefore, when `impl_` needs to be modified use
    // `_get_mutable_impl()` in order to either const-cast or copy
    // the `impl_` depending on its refcount.
    //
    const dt::ColumnImpl* impl_;

  public:
    // Maximum size for an array that can be addressed using `int32_t`
    // indices. This bound is inclusive.
    static constexpr size_t MAX_ARR32_SIZE = 0x7FFFFFFF;

  //------------------------------------
  // Constructors
  //------------------------------------
  // Note: the move-constructor MUST be noexcept; if not then an
  // `std::vector<Column>` will be copying Columns instead of moving
  // when the vector if resized.
  public:
    Column() noexcept;
    Column(const Column&) noexcept;
    Column(Column&&) noexcept;
    Column& operator=(const Column&);
    Column& operator=(Column&&) noexcept;
    ~Column();

    static Column new_data_column(size_t nrows, dt::SType);
    static Column new_na_column(size_t nrows, dt::SType stype);
    static Column new_mbuf_column(size_t nrows, dt::SType, Buffer&&);
    static Column new_string_column(size_t n, Buffer&& data, Buffer&& str);
    static Column from_pybuffer(const py::robj& buffer);
    static Column from_pylist(const py::olist& list, dt::Type);
    static Column from_pylist_of_tuples(const py::olist& list, size_t index, dt::Type);
    static Column from_pylist_of_dicts(const py::olist& list, py::robj name, dt::Type);
    static Column from_range(int64_t start, int64_t stop, int64_t step, dt::Type);
    static Column from_arrow(std::shared_ptr<dt::OArrowArray>&&, const dt::ArrowSchema*);

    // Move-semantics for the pointer here indicates to the user that
    // the class overtakes ownership of that pointer.
    explicit Column(dt::ColumnImpl*&& col) noexcept;

    // Columns are considered equal if they have the same `impl_`.
    bool operator==(const Column& other) const noexcept;

  //------------------------------------
  // Properties
  //------------------------------------
  public:
    size_t nrows() const noexcept;
    size_t na_count() const;
    const dt::Type& type() const noexcept;
    dt::SType stype() const noexcept;
    dt::LType ltype() const noexcept;
    size_t elemsize() const noexcept;
    bool   is_fixedwidth() const noexcept;
    bool   is_virtual() const noexcept;
    bool   is_constant() const noexcept;
    bool   allow_parallel_access() const;
    size_t memory_footprint() const noexcept;
    operator bool() const noexcept;
    dt::ColumnImpl* release() &&;

  //------------------------------------
  // Element access
  //------------------------------------
  public:
    // Each `get_element(i, &out)` function retrieves the column's
    // element at index `i` and stores it in the variable `out`. The
    // return value is true if the i-th element is valid, and false
    // if it is NA (missing).
    // When `false` is returned, the `out` value may contain garbage
    // data, and should not be relied upon.
    //
    // Multiple overloads of `get_element()` correspond to different
    // stypes of the underlying column. It is the caller's
    // responsibility to call the correct function variant; calling
    // a method that doesn't match the column's dt::SType will likely
    // result in an exception thrown.
    //
    // The function expects that `i < nrows()`. This assumption is
    // not checked in production builds.
    //
    bool get_element(size_t i, int8_t* out) const;
    bool get_element(size_t i, int16_t* out) const;
    bool get_element(size_t i, int32_t* out) const;
    bool get_element(size_t i, int64_t* out) const;
    bool get_element(size_t i, float* out) const;
    bool get_element(size_t i, double* out) const;
    bool get_element(size_t i, dt::CString* out) const;
    bool get_element(size_t i, py::oobj* out) const;
    bool get_element(size_t i, Column* out) const;

    // `get_element_as_pyobject(i)` returns the i-th element of the
    // column wrapped into a pyobject of the appropriate type.
    py::oobj get_element_as_pyobject(size_t i) const;

    // Return validity of the i-th element.
    bool get_element_isvalid(size_t i) const;


  //------------------------------------
  // Data buffers
  //------------------------------------
  public:
    // Return the method that this column uses to encode NAs. See
    // the description of `NaStorage` enum for details.
    NaStorage get_na_storage_method() const noexcept;

    // A Column may be comprised of multiple data buffers. For virtual
    // columns this property returns 0.
    size_t get_num_data_buffers() const noexcept;

    // Since the Column contains `n = get_n_data_buffers()` buffers,
    // each of the methods below takes the parameter `k < n` that
    // specifies which buffer the function is applied to.
    // Correspondingly, the methods below are not applicable to
    // virtual columns and will raise an exception.

    // Return true if data buffer can be edited as-is, without
    // creating any extra copies.
    bool is_data_editable(size_t k = 0) const;

    // Access the column's data buffers. The "readonly" version will
    // return the data buffer suitable for reading, while the
    // "editable" method will allow the data to be both read and
    // written. The latter method may need to create a copy of the
    // data buffer.
    size_t      get_data_size(size_t k = 0) const;
    const void* get_data_readonly(size_t k = 0) const;
    void*       get_data_editable(size_t k = 0);
    Buffer      get_data_buffer(size_t k = 0) const;

    size_t n_children() const noexcept;
    const Column& child(size_t i) const;

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
    // object, however Column provides two additional helper functions:
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
    std::unique_ptr<Stats> clone_stats() const;
    void replace_stats(std::unique_ptr<Stats>&&);
    bool is_stat_computed(Stat) const;


  //------------------------------------
  // ColumnImpl manipulation
  //------------------------------------
  public:
    void materialize(bool to_memory = false);
    void rbind(colvec& columns, bool force);
    void cast_inplace(dt::SType stype);
    void cast_inplace(dt::Type type);
    Column cast(dt::SType stype) const;
    Column cast(dt::Type type) const;
    Column reduce_type(bool strict) const;
    void sort_grouped(const Groupby&);

    void replace_values(const RowIndex& replace_at, const Column& replace_with);

    // Rbind the column with itself `ntimes` times.
    void repeat(size_t ntimes);

    // Resize this column to `new_nrows` rows.
    // If the new number of rows is greater than the current number
    // of rows, the column is padded with NAs.
    // If it is less than the current number of rows, the column is
    // truncated.
    void resize(size_t new_nrows);

    // Modifies the column in-place converting it into a view column
    // using the provided RowIndex.
    void apply_rowindex(const RowIndex&);

    // `npmask` is a boolean array of the same length as the column,
    // the true/false values in this mask indicate whether each
    // element of the column is NA (true) or not (false).
    //
    // The `npmask` is the output, not input parameter. This method
    // must fill part of the array from `npmask[row0]` to
    // `npmask[row1 - 1]`. This method is single-threaded (it is
    // designed to be called from multiple threads).
    void fill_npmask(bool* npmask, size_t row0, size_t row1) const;

    // Checks internal integrity of the column. An `AssertionError`
    // exception will be raised if the column's data is in an
    // erroneous state.
    void verify_integrity() const;

    // Save the column's data into the provided data structure `cj`.
    // However, do not call `cj.write()`.
    void save_to_jay(ColumnJayData& cj);

    // See frame/to_arrow.cc
    std::unique_ptr<dt::OArrowArray> to_arrow() const;
    std::unique_ptr<dt::OArrowSchema> to_arrow_schema() const;

    // "Materialize" the column into arrow format
    bool is_arrow() const;
    Column as_arrow() const;

    // A shortcut for `.type().can_be_read_as<T>()`. The latter call
    // is not compatible with some compilers, for instance Clang 11.
    template<typename T> bool can_be_read_as() const {
      return type().can_be_read_as<T>();
    }

    // Replace type of the current column. Use only if you're sure that
    // the column can be safely read with the new type.
    void replace_type_unsafe(dt::Type new_type);

  private:
    void _acquire_impl(const dt::ColumnImpl*);
    void _release_impl(const dt::ColumnImpl*);

    // This method returns const-casted `impl_` pointer. However, it
    // also checks the reference counter on that pointer, and if the
    // count > 1, makes a "shallow" copy of the `impl_`. Thus, it
    // ensures proper copy-on-write semantics.
    //
    // This method should be called from any Column method that
    // intends to modify the `impl_` variable.
    dt::ColumnImpl* _get_mutable_impl(bool keep_stats=false);

    friend void swap(Column& lhs, Column& rhs);
    friend class ColumnImpl;
};



#endif
