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
class Nth_ColumnImpl : public Virtual_ColumnImpl {
  private:
    Column col_;
    Groupby gby_;
    int32_t n_;
    size_t : 32;

  public:
    Nth_ColumnImpl(Column&& col, const Groupby& gby, int32_t n)
      : Virtual_ColumnImpl(gby.size(), col.stype()),
        col_(std::move(col)),
        gby_(gby),
        n_(n)
    {
      xassert(col_.can_be_read_as<T>());
    }


    ColumnImpl* clone() const override {
      return new Nth_ColumnImpl(Column(col_), gby_, n_);
    }


    size_t n_children() const noexcept override {
      return 1;
    }


    const Column& child(size_t i) const override {
      xassert(i == 0);  (void)i;
      return col_;
    }


    bool get_element(size_t i, T* out) const override { 
      xassert(i < gby_.size());
      size_t i0, i1;
      gby_.get_group(i, &i0, &i1);

      // Note, when `n_` is negative it is cast to `size_t`, that is
      // an unsigned type. Then, when adding `i1`, we rely on `size_t`
      // wrap-around.
      size_t ni = (n_ >= 0)? static_cast<size_t>(n_) + i0
                           : static_cast<size_t>(n_) + i1;

      bool isvalid = false;
      if (ni >= i0 && ni < i1){
        if (SKIPNA){
          size_t ii = ni;
          while (ii < i1 && !isvalid) {
            isvalid = col_.get_element(ii, out);
            ii++;
          }
        } else {
          isvalid = col_.get_element(ni, out);
        }
      }
      return isvalid;
    }
};


}  // namespace dt
#endif


