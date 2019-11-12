//------------------------------------------------------------------------------
// Copyright 2018-2019 H2O.ai
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
#include "utils/assert.h"
#include "utils/exceptions.h"
#include "column.h"
#include "groupby.h"



// The default constructor creates an "empty" Groupby: it is the
// groupby that carries no information, and is used to indicate
// absence of any grouping.
//
// Some operations may not be valid for an empty groupby. You can
// check whether the Groupby is empty using `!(groupby)` or
// `groupby.offsets_r() == nullptr`.
//
Groupby::Groupby()
  : offsets_(),
    ngroups_(0) {}


Groupby::Groupby(size_t n, Buffer&& buf) {
  XAssert(buf.size() == sizeof(int32_t) * (n + 1));
  XAssert(buf.get_element<int32_t>(0) == 0);
  offsets_ = std::move(buf);
  ngroups_ = n;
}


Groupby Groupby::zero_groups() {
  Buffer mr = Buffer::mem(sizeof(int32_t));
  mr.set_element<int32_t>(0, 0);
  return Groupby(0, std::move(mr));
}


Groupby Groupby::single_group(size_t nrows) {
  XAssert(nrows <= Column::MAX_ARR32_SIZE);
  Buffer mr = Buffer::mem(2 * sizeof(int32_t));
  mr.set_element<int32_t>(0, 0);
  mr.set_element<int32_t>(1, static_cast<int32_t>(nrows));
  return Groupby(1, std::move(mr));
}


// Returns true for a valid Groupby object, and false the default-
// constructed "empty" Groupby.
//
Groupby::operator bool() const {
  return bool(offsets_);
}


size_t Groupby::size() const noexcept {
  return ngroups_;
}


const int32_t* Groupby::offsets_r() const {
  return static_cast<const int32_t*>(offsets_.rptr());
}


size_t Groupby::last_offset() const noexcept {
  const int32_t* offs = offsets_r();
  return offs? static_cast<size_t>(offs[ngroups_]) : 0;
}


void Groupby::get_group(size_t i, size_t* i0, size_t* i1) const {
  const int32_t* offsets_array = offsets_r();
  *i0 = static_cast<size_t>(offsets_array[i]);
  *i1 = static_cast<size_t>(offsets_array[i + 1]);
}


RowIndex Groupby::ungroup_rowindex() {
  const int32_t* offs = offsets_r();
  if (!offs) return RowIndex();
  int32_t nrows = offs[ngroups_];
  arr32_t indices(static_cast<size_t>(nrows));
  int32_t* data = indices.data();
  int32_t j = 0;
  for (size_t i = 0; i < ngroups_; ++i) {
    int32_t upto = offs[i + 1];
    int32_t ii = static_cast<int32_t>(i);
    while (j < upto) data[j++] = ii;
  }
  return RowIndex(std::move(indices), /* sorted = */ true);
}
