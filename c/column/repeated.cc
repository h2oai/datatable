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
#include "column/const.h"
#include "column/repeated.h"
namespace dt {



//------------------------------------------------------------------------------
// Repeated_ColumnImpl
//------------------------------------------------------------------------------

Repeated_ColumnImpl::Repeated_ColumnImpl(Column&& col, size_t ntimes)
  : ColumnImpl(col.nrows() * ntimes, col.stype()),
    mod(col.nrows()),
    arg(std::move(col))
{
  if (mod == 0) mod = 1;
}


bool Repeated_ColumnImpl::is_virtual() const noexcept {
  return true;
}

ColumnImpl* Repeated_ColumnImpl::shallowcopy() const {
  return new Repeated_ColumnImpl(Column(arg), _nrows / mod);
}

void Repeated_ColumnImpl::repeat(size_t ntimes, bool inplace, Column& out) {
  if (inplace) {
    _nrows *= ntimes;
  }
  else {
    out = Column(new Repeated_ColumnImpl(Column(arg), _nrows * ntimes / mod));
  }
}



bool Repeated_ColumnImpl::get_element(size_t i, int8_t* out)   const { return arg.get_element(i % mod, out); }
bool Repeated_ColumnImpl::get_element(size_t i, int16_t* out)  const { return arg.get_element(i % mod, out); }
bool Repeated_ColumnImpl::get_element(size_t i, int32_t* out)  const { return arg.get_element(i % mod, out); }
bool Repeated_ColumnImpl::get_element(size_t i, int64_t* out)  const { return arg.get_element(i % mod, out); }
bool Repeated_ColumnImpl::get_element(size_t i, float* out)    const { return arg.get_element(i % mod, out); }
bool Repeated_ColumnImpl::get_element(size_t i, double* out)   const { return arg.get_element(i % mod, out); }
bool Repeated_ColumnImpl::get_element(size_t i, CString* out)  const { return arg.get_element(i % mod, out); }
bool Repeated_ColumnImpl::get_element(size_t i, py::robj* out) const { return arg.get_element(i % mod, out); }




}  // namespace dt

// This is the base implementation of the virtual
// `ColumnImpl::repeat()` method. In this implementation the
// `inplace` flag is ignored, because we cannot make an arbitrary
// column repeated in-place.
//
// Instead, we replace this object with a `Repeated_ColumnImpl` in
// general, or with a `Const_ColumnImpl` in a special case when the
// current column has only 1 row.
//
void ColumnImpl::repeat(size_t ntimes, bool inplace, Column& out) {
  (void) inplace;
  if (nrows() == 1) {
    // Note: Const_ColumnImpl overrides the `repeat()` method. If it
    // didn't, we would have had an infinite recursion here...
    out = dt::Const_ColumnImpl::from_1row_column(out);
    out.repeat(ntimes);
  }
  else {
    out = Column(new dt::Repeated_ColumnImpl(std::move(out), ntimes));
  }
}
