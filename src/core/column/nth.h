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
#ifndef dt_NTH_h
#define dt_NTH_h
#include "column/virtual.h"
#include "stype.h"

namespace dt {
template<typename T, bool SKIPNA>
class NTH_ColumnImpl : public Virtual_ColumnImpl {
  private:
    Column col_;
    int32_t nth_;
    Groupby gby_;

  public:
    NTH_ColumnImpl(Column&& col, int32_t nth,  const Groupby& gby)
      : Virtual_ColumnImpl(gby.size(), col.stype()),
        col_(std::move(col)),
        nth_(nth),        
        gby_(gby)
      {xassert(col_.can_be_read_as<T>());}

    ColumnImpl* clone() const override {
      return new NTH_ColumnImpl(Column(col_), nth_, gby_);
    }

    size_t n_children() const noexcept override {
      return 1;
    }

    const Column& child(size_t i) const override {
      xassert(i == 0);  (void)i;
      return col_;
    }

    bool get_element(size_t i, T* out) const override{ 
      size_t n;          
      size_t i0, i1;
      gby_.get_group(i, &i0, &i1);
      xassert(i0 < i1);
      n = nth_<0 ? static_cast<size_t>(nth_)+i1
                 : static_cast<size_t>(nth_)+i0;
      bool isvalid = false;
      if (n >= i0 & n< i1){
        for (size_t ii = n; ii < i1; ++ii) {
          isvalid = col_.get_element(ii, out);
          if (isvalid || !SKIPNA) {break;}
        }
      }
      if (isvalid){
        return true;
      }
      else {*out = GETNA<T>();}
      return false;
    }
};


}  // namespace dt


#endif