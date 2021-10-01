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
#include "column/nafilled.h"
#include "column/truncated.h"
namespace dt {


Truncated_ColumnImpl::Truncated_ColumnImpl(Column&& col, size_t nrows)
  : Virtual_ColumnImpl(nrows, col.stype()),
    arg_(std::move(col))
{
  xassert(nrows < arg_.nrows());
}


ColumnImpl* Truncated_ColumnImpl::clone() const {
  return new Truncated_ColumnImpl(Column(arg_), nrows_);
}


void Truncated_ColumnImpl::na_pad(size_t new_nrows, Column& out) {
  xassert(new_nrows > nrows_);
  out = Column(new NaFilled_ColumnImpl(std::move(arg_), new_nrows, nrows_));
}

void Truncated_ColumnImpl::truncate(size_t new_nrows, Column&)
{
  xassert(new_nrows < nrows_);
  nrows_ = new_nrows;
}

size_t Truncated_ColumnImpl::n_children() const noexcept {
  return 1;
}

const Column& Truncated_ColumnImpl::child(size_t i) const {
  xassert(i == 0);  (void)i;
  return arg_;
}



template <typename S>
bool Truncated_ColumnImpl::_get_element(size_t i, S* out) const {
  xassert(i < nrows_);
  return arg_.get_element(i, out);
}


bool Truncated_ColumnImpl::get_element(size_t i, int8_t* out)   const { return _get_element(i, out); }
bool Truncated_ColumnImpl::get_element(size_t i, int16_t* out)  const { return _get_element(i, out); }
bool Truncated_ColumnImpl::get_element(size_t i, int32_t* out)  const { return _get_element(i, out); }
bool Truncated_ColumnImpl::get_element(size_t i, int64_t* out)  const { return _get_element(i, out); }
bool Truncated_ColumnImpl::get_element(size_t i, float* out)    const { return _get_element(i, out); }
bool Truncated_ColumnImpl::get_element(size_t i, double* out)   const { return _get_element(i, out); }
bool Truncated_ColumnImpl::get_element(size_t i, CString* out)  const { return _get_element(i, out); }
bool Truncated_ColumnImpl::get_element(size_t i, py::oobj* out) const { return _get_element(i, out); }
bool Truncated_ColumnImpl::get_element(size_t i, Column* out)   const { return _get_element(i, out); }




}  // namespace dt
