//------------------------------------------------------------------------------
// Copyright 2023 H2O.ai
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
#ifndef dt_COLUMN_REDUCE_NULLARY_h
#define dt_COLUMN_REDUCE_NULLARY_h
#include "column/virtual.h"
#include "stype.h"
namespace dt {


class ReduceNullary_ColumnImpl : public Virtual_ColumnImpl {
  protected:
    Groupby gby_;
    

  public:
    ReduceNullary_ColumnImpl(const Groupby& gby, SType stype)
      : Virtual_ColumnImpl(gby.size(), stype),
        gby_(gby)
    {}


    ColumnImpl *clone() const override {
      return new ReduceNullary_ColumnImpl(Groupby(gby_), this->stype());
    }

    size_t n_children() const noexcept override {
      return 0;
    }

};


}  // namespace dt
#endif
