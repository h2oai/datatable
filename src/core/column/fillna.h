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
#ifndef dt_COLUMN_FILLNA_h
#define dt_COLUMN_FILLNA_h
#include "column/virtual.h"
#include "stype.h"
namespace dt {

template<typename T>
class FillNA_ColumnImpl : public Virtual_ColumnImpl {
  private:
    Column&& col_;
    Column&& value_;

  public:
    FillNA_ColumnImpl(Column&& col, Column&& value)
      : Virtual_ColumnImpl(col.nrows(), value.stype()),
        col_(std::move(col)),
        value_(std::move(value))
    {
        xassert(col.stype() == value.stype());
        xassert(col.nrows() == value.nrows());

    }


    ColumnImpl* clone() const override {
      return new FillNA_ColumnImpl(Column(col_), Column(value_));
    }


    size_t n_children() const noexcept override {
      return 1;
    }

    const Column& child(size_t i) const override {
      xassert(i == 0);  (void)i;
      return col_;
    }

    bool get_element(size_t i, T* out) const override {
        T orig_value;
        T replacement;
        bool valid = col_.get_element(i, &orig_value);
        col_.get_element(i, &replacement);
        *out = valid?orig_value:replacement;
        return true;
    }


};


}  // namespace dt


#endif
