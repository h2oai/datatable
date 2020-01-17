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
#ifndef dt_COLUMN_ISCLOSE_h
#define dt_COLUMN_ISCLOSE_h
#include <memory>
#include "column/virtual.h"
#include "utils/assert.h"
namespace dt {


/**
  *
  */
template <typename T>
class IsClose_ColumnImpl : public Virtual_ColumnImpl {
  private:
    Column argx_;
    Column argy_;
    T rtol_;
    T atol_;

  public:
    IsClose_ColumnImpl(Column&& xcol, Column&& ycol,
                       T rtol, T atol, size_t nrows)
      : Virtual_ColumnImpl(nrows, SType::BOOL),
        argx_(std::move(xcol)),
        argy_(std::move(ycol)),
        rtol_(rtol),
        atol_(atol)
    {
      assert_compatible_type<T>(argx_.stype());
      assert_compatible_type<T>(argy_.stype());
    }

    ColumnImpl* clone() const override {
      return new IsClose_ColumnImpl<T>(Column(argx_), Column(argy_),
                                       rtol_, atol_, nrows_);
    }

    bool allow_parallel_access() const override {
      return argx_.allow_parallel_access() && argy_.allow_parallel_access();
    }


    bool get_element(size_t i, int8_t* out) const override {
      T x, y;
      bool xvalid = argx_.get_element(i, &x);
      bool yvalid = argy_.get_element(i, &y);
      if (!xvalid || !yvalid) {
        *out = (xvalid == yvalid);
      } else {
        *out = (x == y) || (std::abs(x - y) <= atol_ + rtol_ * std::abs(y));
      }
      return true;
    }
};




}  // namespace dt
#endif
