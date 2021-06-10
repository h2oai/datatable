//------------------------------------------------------------------------------
// Copyright 2021 H2O.ai
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
#include "column/time_scaled.h"
#include "stype.h"
namespace dt {


TimeScaled_ColumnImpl::TimeScaled_ColumnImpl(Column&& arg, int64_t scale)
  : Virtual_ColumnImpl(arg.nrows(), dt::SType::TIME64),
    arg_(std::move(arg)),
    scale_(scale)
{
  xassert(arg_.can_be_read_as<int64_t>());
}


ColumnImpl* TimeScaled_ColumnImpl::clone() const {
  return new TimeScaled_ColumnImpl(Column(arg_), scale_);
}


size_t TimeScaled_ColumnImpl::n_children() const noexcept {
  return 1;
}


const Column& TimeScaled_ColumnImpl::child(size_t) const {
  return arg_;
}



bool TimeScaled_ColumnImpl::get_element(size_t i, int64_t* out) const {
  int64_t x;
  bool isvalid = arg_.get_element(i, &x);
  *out = x * scale_;
  return isvalid;
}




}  // namespace dt
