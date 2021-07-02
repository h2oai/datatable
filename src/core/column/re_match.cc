//------------------------------------------------------------------------------
// Copyright 2019-2021 H2O.ai
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
#include "column/re_match.h"
namespace dt {



Re_Match_ColumnImpl::Re_Match_ColumnImpl(Column&& col, const std::regex& rx)
  : Virtual_ColumnImpl(col.nrows(), SType::BOOL),
    arg_(std::move(col)),
    regex_(rx) {}


ColumnImpl* Re_Match_ColumnImpl::clone() const {
  return new Re_Match_ColumnImpl(Column(arg_), regex_);
}


size_t Re_Match_ColumnImpl::n_children() const noexcept {
  return 1;
}

const Column& Re_Match_ColumnImpl::child(size_t i) const {
  xassert(i == 0);  (void)i;
  return arg_;
}


bool Re_Match_ColumnImpl::get_element(size_t i, int8_t* out) const {
  CString x;
  bool isvalid = arg_.get_element(i, &x);
  if (isvalid) {
    *out = std::regex_match(x.data(), x.end(), regex_);
  }
  return isvalid;
}




}  // namespace dt
