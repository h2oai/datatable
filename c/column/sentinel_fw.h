//------------------------------------------------------------------------------
// Copyright 2019 H2O.ai
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
#ifndef dt_COLUMN_SENTINEL_FW_h
#define dt_COLUMN_SENTINEL_FW_h
#include "column/sentinel.h"
// namespace dt {



template <typename T>
class FwColumn : public dt::Sentinel_ColumnImpl
{
  protected:
    Buffer mbuf;

  public:
    FwColumn();
    FwColumn(ColumnImpl*&&);
    FwColumn(size_t nrows);
    FwColumn(size_t nrows, Buffer&&);

    const T* elements_r() const;
    T* elements_w();
    T get_elem(size_t i) const;

    virtual bool get_element(size_t i, T* out) const override;

    virtual ColumnImpl* shallowcopy() const override;
    virtual ColumnImpl* materialize() override;
    size_t get_num_data_buffers() const noexcept override;
    bool is_data_editable(size_t k) const override;
    size_t get_data_size(size_t k) const override;
    const void* get_data_readonly(size_t k) const override;
    void*       get_data_editable(size_t k) override;
    Buffer      get_data_buffer(size_t k) const override;

    void replace_values(const RowIndex& at, const Column& with, Column&) override;
    void replace_values(const RowIndex& at, T with);
    size_t memory_footprint() const noexcept override;
    void verify_integrity() const override;

  protected:
    void rbind_impl(colvec& columns, size_t nrows, bool isempty) override;

    friend ColumnImpl;
};


extern template class FwColumn<int8_t>;
extern template class FwColumn<int16_t>;
extern template class FwColumn<int32_t>;
extern template class FwColumn<int64_t>;
extern template class FwColumn<float>;
extern template class FwColumn<double>;
extern template class FwColumn<py::robj>;



//==============================================================================

class BoolColumn : public FwColumn<int8_t>
{
  public:
    BoolColumn(ColumnImpl*&&);
    BoolColumn(size_t nrows = 0);
    BoolColumn(size_t nrows, Buffer&&);
    ColumnImpl* shallowcopy() const override;

    using FwColumn<int8_t>::get_element;
    bool get_element(size_t i, int32_t* out) const override;

  protected:
    void verify_integrity() const override;

    friend ColumnImpl;
};



//==============================================================================

template <typename T>
class IntColumn : public FwColumn<T>
{
  public:
    using FwColumn<T>::FwColumn;
    ColumnImpl* shallowcopy() const override;

    using FwColumn<T>::get_element;
    bool get_element(size_t i, int32_t* out) const override;
    bool get_element(size_t i, int64_t* out) const override;

  protected:
    using ColumnImpl::stats_;
    friend ColumnImpl;
};

extern template class IntColumn<int8_t>;
extern template class IntColumn<int16_t>;
extern template class IntColumn<int32_t>;
extern template class IntColumn<int64_t>;




//==============================================================================

/**
 * ColumnImpl containing `PyObject*`s.
 *
 * This column is a fall-back for implementing types that cannot be normally
 * supported by other columns. Manipulations with this column almost invariably
 * go through Python runtime, and hence are single-threaded and slow.
 *
 * The `mbuf` array for this ColumnImpl must be marked as "pyobjects" (see
 * documentation for Buffer). In practice it means that:
 *   * Only real python objects may be stored, not NULL pointers.
 *   * All stored `PyObject*`s must have their reference counts incremented.
 *   * When a value is removed or replaced in `mbuf`, it should be decref'd.
 * The `mbuf`'s API already respects these rules, however the user must also
 * obey them when manipulating the data manually.
 */
class PyObjectColumn : public FwColumn<py::robj>
{
public:
  PyObjectColumn();
  PyObjectColumn(size_t nrows);
  PyObjectColumn(size_t nrows, Buffer&&);
  ColumnImpl* shallowcopy() const override;
  ColumnImpl* materialize() override;

  bool get_element(size_t i, py::robj* out) const override;

protected:
  void rbind_impl(colvec& columns, size_t nrows, bool isempty) override;
  void verify_integrity() const override;
  friend ColumnImpl;
};



// }  // namespace dt
#endif
