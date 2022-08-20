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
#ifndef dt_COLUMN_CUMCOUNT_h
#define dt_COLUMN_CUMCOUNT_h
#include "column/virtual.h"

namespace dt {


class FExpr_Alias : FExpr_Func {
  private:
    ptrExpr arg_;
    strvec names_;

  public:
    FExpr_Alias(ptrExpr&& arg, strvec&& names)
      : arg_(std::move(arg)), 
        names_(std::move(names))
        {}

    ColumnImpl* clone() const override {
      return new FExpr_Alias(arg_, names_);
    }

    size_t n_children() const noexcept override {
      return 0;
    }

    const Column& child(size_t i) const override {
      xassert(i == 0);  (void)i;
    }

    bool get_element(size_t, int8_t*)  const override;
    bool get_element(size_t, int16_t*) const override;
    bool get_element(size_t, int32_t*) const override;
    bool get_element(size_t, int64_t*) const override;
    bool get_element(size_t, float*)   const override;
    bool get_element(size_t, double*)  const override;
    bool get_element(size_t i, CString*  out) const override;
    bool get_element(size_t i, py::oobj* out) const override;

};


}  // namespace dt


#endif
