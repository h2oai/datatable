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
#ifndef dt_COLUMN_STRVEC_h
#define dt_COLUMN_STRVEC_h
#include <string>
#include <vector>
#include "column/virtual.h"
namespace dt {



/**
  * This class treats a simple vector of strings as if it was a
  * STR32 column.
  *
  * Note that the vector is stored by reference, and it is therefore
  * the responsibility of the user to keep the source vector alive
  * for the duration of the lifetime of this object.
  */
class Strvec_ColumnImpl : public Virtual_ColumnImpl {
  private:
    const strvec& vec;

  public:
    Strvec_ColumnImpl(const strvec& v)
      : Virtual_ColumnImpl(v.size(), SType::STR32),
        vec(v) {}

    bool get_element(size_t i, CString* out) const override {
      out->ch = vec[i].c_str();
      out->size = static_cast<int64_t>(vec[i].size());
      return true;
    }

    ColumnImpl* clone() const override {
      auto res = new Strvec_ColumnImpl(vec);
      res->nrows_ = nrows_;
      return res;
    }
};




}  // namespace dt
#endif
