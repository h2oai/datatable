//------------------------------------------------------------------------------
// Copyright 2019-2021 H2O.ai
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
#ifndef dt_COLUMN_ISNA_h
#define dt_COLUMN_ISNA_h
#include "column/const.h"
#include "column/virtual.h"
#include "stype.h"
namespace dt {


template <typename T>
class Isna_ColumnImpl : public Virtual_ColumnImpl {
  protected:
    Column arg_;

  public:
    Isna_ColumnImpl(Column&& col)
      : Virtual_ColumnImpl(col.nrows(), SType::BOOL),
        arg_(std::move(col))
    {
      xassert(arg_.can_be_read_as<T>());
    }


    ColumnImpl* clone() const override {
      return new Isna_ColumnImpl<T>(Column(arg_));
    }

    size_t n_children() const noexcept override {
      return 1;
    }

    const Column& child(size_t i) const override {
      xassert(i == 0);  (void)i;
      return arg_;
    }


    bool get_element(size_t i, int8_t* out) const override {
      T tmp;
      *out = ! arg_.get_element(i, &tmp);
      return true;
    }
};


static Column make_isna_col(Column&& col) {
  switch (col.stype()) {
    case SType::VOID:    return Const_ColumnImpl::make_bool_column(col.nrows(), true);
    case SType::BOOL:
    case SType::INT8:    return Column(new Isna_ColumnImpl<int8_t>(std::move(col)));
    case SType::INT16:   return Column(new Isna_ColumnImpl<int16_t>(std::move(col)));
    case SType::DATE32:
    case SType::INT32:   return Column(new Isna_ColumnImpl<int32_t>(std::move(col)));
    case SType::TIME64:
    case SType::INT64:   return Column(new Isna_ColumnImpl<int64_t>(std::move(col)));
    case SType::FLOAT32: return Column(new Isna_ColumnImpl<float>(std::move(col)));
    case SType::FLOAT64: return Column(new Isna_ColumnImpl<double>(std::move(col)));
    case SType::STR32:
    case SType::STR64:   return Column(new Isna_ColumnImpl<CString>(std::move(col)));
    default: throw RuntimeError();
  }
}

}  // namespace dt
#endif

