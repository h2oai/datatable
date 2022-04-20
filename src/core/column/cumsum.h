//------------------------------------------------------------------------------
// Copyright 2022 H2O.ai
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
#ifndef dt_COLUMN_CUMSUM_h
#define dt_COLUMN_CUMSUM_h
#include "column/virtual.h"
#include "models/utils.h"
#include "parallel/api.h"
#include "stype.h"


template <typename T>
class Cumsum_ColumnImpl : public Virtual_ColumnImpl {
  private:
    Column col_;

  public:
    Cumsum_ColumnImpl(Column&& col)
      : Virtual_ColumnImpl(col.nrows(), col.stype()),
        col_(std::move(col))
    {
      xassert(col_.can_be_read_as<T>());
    }

    template <typename T, U>
    void _materialize(Column& out, bool) override {
      Column tmp = Column::new_data_column(col_.nrows(), stype());
      auto data = static_cast<T*>(tmp.get_data_editable());

      parallel_for_static(col_.nrows(), [&](size_t i) {

        for (size_t i=0; i < col_.nrows(); i++) {
          T val;
          bool val_valid = col_.get_element(i, &val);
          xassert(val_valid); (void) val_valid;
          data[i] = (i == 0) ? val : data[i] + val;
        }

      }

      );

      out = tmp;
    }


    void Cumsum_ColumnImpl::materialize(Column& out, bool) {
      switch (stype()) {
        case SType::BOOL:
        case SType::INT8:    return _materialize<int8_t, SType::INT64>(out);
        case SType::INT16:   return _materialize<int16_t, SType::INT64>(out);
        case SType::INT32:   return _materialize<int32_t, SType::INT64>(out);
        case SType::INT64:   return _materialize<int64_t, SType::INT64>(out);
        case SType::FLOAT32: return _materialize<float, SType::FLOAT32>(out);
        case SType::FLOAT64: return _materialize<double, SType::FLOAT64>(out);
        default:
          throw RuntimeError() << "Invalid stype for a cumsum column: " << stype();
      }
    }


    ColumnImpl* clone() const override {
      return new Cumsum_ColumnImpl(Column(col_));
    }

    size_t n_children() const noexcept override {
      return 1;
    }

    const Column& child(size_t i) const override {
      xassert(i == 0);  (void)i;
      return col_;
    }

};
