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
#ifndef dt_COLUMN_SENTINEL_STR_h
#define dt_COLUMN_SENTINEL_STR_h
#include "column/sentinel.h"
// namespace dt {


template <typename T>
class StringColumn : public dt::Sentinel_ColumnImpl
{
  Buffer strbuf;

public:
  StringColumn();
  StringColumn(size_t nrows);
  StringColumn(size_t nrows, Buffer&& offbuf, Buffer&& strbuf);

  ColumnImpl* materialize() override;
  void apply_na_mask(const Column& mask) override;

  Buffer str_buf() const { return strbuf; }
  size_t datasize() const;
  const char* strdata() const;
  const uint8_t* ustrdata() const;
  const T* offsets() const;
  T* offsets_w();
  size_t memory_footprint() const override;
  const void* data2() const override { return strbuf.rptr(); }
  size_t data2_size() const override {
    return static_cast<const T*>(mbuf.rptr())[_nrows] & ~GETNA<T>();
  }

  ColumnImpl* shallowcopy() const override;
  void replace_values(Column& thiscol, const RowIndex& at, const Column& with) override;

  void verify_integrity(const std::string& name) const override;

  bool get_element(size_t i, CString* out) const override;

protected:
  void rbind_impl(colvec& columns, size_t nrows, bool isempty) override;

  friend ColumnImpl;
  friend Column;
};


extern template class StringColumn<uint32_t>;
extern template class StringColumn<uint64_t>;



// }  // namespace dt
#endif
