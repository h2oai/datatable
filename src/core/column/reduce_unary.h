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
#ifndef dt_COLUMN_REDUCE_UNARY_h
#define dt_COLUMN_REDUCE_UNARY_h
#include "column/virtual.h"
#include "stype.h"
namespace dt {


template <typename T, typename U, bool IS_GROUPED>
class ReduceUnary_ColumnImpl : public Virtual_ColumnImpl {
  protected:
    Column col_;
    Groupby gby_;
    

  public:
    ReduceUnary_ColumnImpl(Column &&col, const Groupby& gby)
      : Virtual_ColumnImpl(gby.size(), stype_from<U>),
        col_(std::move(col)),
        gby_(gby)
    {
      xassert(col_.can_be_read_as<T>());
    }


    ColumnImpl *clone() const override {
      return new ReduceUnary_ColumnImpl(Column(col_), Groupby(gby_));
    }


    size_t n_children() const noexcept override {
      return 1;
    }


    const Column &child(size_t i) const override {
      xassert(i == 0);
      (void)i;
      return col_;
    }
};


}  // namespace dt
#endif
