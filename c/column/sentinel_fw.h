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
namespace dt {



//------------------------------------------------------------------------------
// SentinelFw_ColumnImpl<T>
//------------------------------------------------------------------------------

template <typename T>
class SentinelFw_ColumnImpl : public Sentinel_ColumnImpl
{
  protected:
    Buffer mbuf_;

  public:
    SentinelFw_ColumnImpl(size_t nrows);
    SentinelFw_ColumnImpl(size_t nrows, Buffer&&);
    SentinelFw_ColumnImpl(ColumnImpl*&&);

    virtual ColumnImpl* clone() const override;
    void verify_integrity() const override;
    void materialize(Column&, bool) override;
    size_t memory_footprint() const noexcept override;

    virtual bool get_element(size_t i, int8_t* out) const override;
    virtual bool get_element(size_t i, int16_t* out) const override;
    virtual bool get_element(size_t i, int32_t* out) const override;
    virtual bool get_element(size_t i, int64_t* out) const override;
    virtual bool get_element(size_t i, float* out) const override;
    virtual bool get_element(size_t i, double* out) const override;
    virtual bool get_element(size_t i, py::robj* out) const override;

    size_t      get_num_data_buffers() const noexcept override;
    bool        is_data_editable(size_t k) const override;
    size_t      get_data_size(size_t k) const override;
    const void* get_data_readonly(size_t k) const override;
    void*       get_data_editable(size_t k) override;
    Buffer      get_data_buffer(size_t k) const override;

    void replace_values(const RowIndex& at, const Column& with, Column&) override;
    void replace_values(const RowIndex& at, T with);

  protected:
    void rbind_impl(colvec& columns, size_t nrows, bool isempty) override;
};


extern template class SentinelFw_ColumnImpl<int8_t>;
extern template class SentinelFw_ColumnImpl<int16_t>;
extern template class SentinelFw_ColumnImpl<int32_t>;
extern template class SentinelFw_ColumnImpl<int64_t>;
extern template class SentinelFw_ColumnImpl<float>;
extern template class SentinelFw_ColumnImpl<double>;
extern template class SentinelFw_ColumnImpl<py::robj>;




//------------------------------------------------------------------------------
// SentinelBool_ColumnImpl
//------------------------------------------------------------------------------

class SentinelBool_ColumnImpl : public SentinelFw_ColumnImpl<int8_t>
{
  public:
    SentinelBool_ColumnImpl(ColumnImpl*&&);
    SentinelBool_ColumnImpl(size_t nrows);
    SentinelBool_ColumnImpl(size_t nrows, Buffer&&);
    ColumnImpl* clone() const override;

  protected:
    void verify_integrity() const override;
};




//------------------------------------------------------------------------------
// SentinelObj_ColumnImpl
//------------------------------------------------------------------------------

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
class SentinelObj_ColumnImpl : public SentinelFw_ColumnImpl<py::robj>
{
public:
  SentinelObj_ColumnImpl(size_t nrows);
  SentinelObj_ColumnImpl(size_t nrows, Buffer&&);
  ColumnImpl* clone() const override;

  bool get_element(size_t i, py::robj* out) const override;

protected:
  void rbind_impl(colvec& columns, size_t nrows, bool isempty) override;
  void verify_integrity() const override;
};



}  // namespace dt
#endif
