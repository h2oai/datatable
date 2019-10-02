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
#include <memory>        // std::unique_ptr
#include "python/obj.h"  // py::robj
#include "column.h"      // Column, NaStorage, colvec
#include "groupby.h"     // Groupby
#include "buffer.h"      // Buffer
#include "stats.h"       // Stats
#include "types.h"       // SType, CString


/**
 * A single column within a DataTable.
 *
 * This class serves as the base in the hierarchy of different
 * column implementation classes.
 */
class ColumnImpl
{
  protected:
    size_t nrows_;
    SType  stype_;
    size_t : 56;
    mutable std::unique_ptr<Stats> stats_;

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

    virtual ColumnImpl* shallowcopy() const = 0;
    virtual ColumnImpl* materialize();
    virtual void verify_integrity() const;


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
    virtual size_t memory_footprint() const noexcept = 0;


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


  //------------------------------------
  // Column manipulation
  //------------------------------------
  public:
    virtual void fill_npmask(bool* outmask, size_t row0, size_t row1) const;
    virtual RowIndex sort(Groupby* out_groups) const;
    virtual void sort_grouped(const Groupby&, Column& out);

    virtual void repeat(size_t ntimes, Column& out);
    virtual void na_pad(size_t new_nrows, Column& out);
    virtual void truncate(size_t new_nrows, Column& out);
    virtual void apply_rowindex(const RowIndex& ri, Column& out);
    virtual void replace_values(const RowIndex& replace_at,
                                const Column& replace_with, Column& out);
    virtual void pre_materialize_hook() {}


  protected:
    virtual void rbind_impl(colvec& columns, size_t nrows, bool isempty);

    template <typename T> ColumnImpl* _materialize_fw();
    ColumnImpl* _materialize_str();
    ColumnImpl* _materialize_obj();

    template <typename T>
    void _fill_npmask(bool* outmask, size_t row0, size_t row1) const;

    friend class Column;
};




#endif
