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
#include <memory>
#include "column/nafilled.h"
namespace dt {


NaFilled_ColumnImpl::NaFilled_ColumnImpl(Column&& col, size_t nrows)
  : Virtual_ColumnImpl(nrows, col.stype()),
    arg_nrows(col.nrows()),
    arg(std::move(col))
{
  xassert(nrows >= arg.nrows());
}


ColumnImpl* NaFilled_ColumnImpl::clone() const {
  return new NaFilled_ColumnImpl(Column(arg), nrows_);
}


void NaFilled_ColumnImpl::na_pad(size_t new_nrows, Column&) {
  xassert(new_nrows >= nrows_);
  nrows_ = new_nrows;
}

void NaFilled_ColumnImpl::truncate(size_t new_nrows, Column& out)
{
  xassert(new_nrows < nrows_);
  if (new_nrows < arg_nrows) {
    arg.resize(new_nrows);
    out = std::move(arg);
  }
  else {
    nrows_ = new_nrows;
  }
}




bool NaFilled_ColumnImpl::get_element(size_t i, int8_t* out)   const {
  return (i < arg_nrows) && arg.get_element(i, out);
}

bool NaFilled_ColumnImpl::get_element(size_t i, int16_t* out)  const {
  return (i < arg_nrows) && arg.get_element(i, out);
}

bool NaFilled_ColumnImpl::get_element(size_t i, int32_t* out)  const {
  return (i < arg_nrows) && arg.get_element(i, out);
}

bool NaFilled_ColumnImpl::get_element(size_t i, int64_t* out)  const {
  return (i < arg_nrows) && arg.get_element(i, out);
}

bool NaFilled_ColumnImpl::get_element(size_t i, float* out)    const {
  return (i < arg_nrows) && arg.get_element(i, out);
}

bool NaFilled_ColumnImpl::get_element(size_t i, double* out)   const {
  return (i < arg_nrows) && arg.get_element(i, out);
}

bool NaFilled_ColumnImpl::get_element(size_t i, CString* out)  const {
  return (i < arg_nrows) && arg.get_element(i, out);
}

bool NaFilled_ColumnImpl::get_element(size_t i, py::robj* out) const {
  return (i < arg_nrows) && arg.get_element(i, out);
}





}  // namespace dt
