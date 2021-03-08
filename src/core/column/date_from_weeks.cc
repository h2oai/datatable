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
#include "column/date_from_weeks.h"
#include "stype.h"
namespace dt {


DateFromWeeks_ColumnImpl::DateFromWeeks_ColumnImpl(Column&& arg)
  : Virtual_ColumnImpl(arg.nrows(), SType::DATE32),
    arg_(std::move(arg))
{
  xassert(arg_.can_be_read_as<int64_t>());
}


bool DateFromWeeks_ColumnImpl::get_element(size_t i, int32_t* out) const {
  int64_t value;
  bool valid = arg_.get_element(i, &value);
  *out = static_cast<int32_t>(value * 7);
  return valid;
}


ColumnImpl* DateFromWeeks_ColumnImpl::clone() const {
  return new DateFromWeeks_ColumnImpl(Column(arg_));
}

size_t DateFromWeeks_ColumnImpl::n_children() const noexcept {
  return 1;
}

const Column& DateFromWeeks_ColumnImpl::child(size_t i) const {
  xassert(i == 0); (void) i;
  return arg_;
}




}
