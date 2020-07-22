//------------------------------------------------------------------------------
// Copyright 2020 H2O.ai
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
#include <cstring>
#include "column/string_plus.h"
#include "ltype.h"
#include "stype.h"
namespace dt {


StringPlus_ColumnImpl::StringPlus_ColumnImpl(Column&& col1, Column&& col2)
  : Virtual_ColumnImpl(col1.nrows(), SType::STR32),
    col1_(std::move(col1)),
    col2_(std::move(col2))
{
  xassert(col1_.nrows() == col2_.nrows());
  xassert(col1_.ltype() == LType::STRING);
  xassert(col2_.ltype() == LType::STRING);
}


ColumnImpl* StringPlus_ColumnImpl::clone() const {
  return new StringPlus_ColumnImpl(Column(col1_), Column(col2_));
}


size_t StringPlus_ColumnImpl::n_children() const noexcept {
  return 2;
}

const Column& StringPlus_ColumnImpl::child(size_t i) const {
  xassert(i <= 1);
  return i == 0? col1_ : col2_;
}


bool StringPlus_ColumnImpl::get_element(size_t i, CString* out) const {
  CString lstr;
  bool isvalid = col1_.get_element(i, &lstr);
  if (isvalid) {
    CString rstr;
    isvalid = col2_.get_element(i, &rstr);
    if (isvalid) {
      size_t lhs_size = lstr.size();
      size_t rhs_size = rstr.size();
      char* ptr = out->prepare_buffer(lhs_size + rhs_size);
      if (lhs_size) std::memcpy(ptr, lstr.data(), lhs_size);
      if (rhs_size) std::memcpy(ptr + lhs_size, rstr.data(), rhs_size);
      xassert(!out->isna());
    }
  }
  return isvalid;
}



}  // namespace dt
