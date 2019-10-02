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
#include "column/virtual.h"
namespace dt {


Virtual_ColumnImpl::Virtual_ColumnImpl(size_t nrows, SType stype)
  : ColumnImpl(nrows, stype) {}


bool Virtual_ColumnImpl::is_virtual() const noexcept {
  return true;
}

size_t Virtual_ColumnImpl::memory_footprint() const noexcept {
  return sizeof(*this);
}



NaStorage Virtual_ColumnImpl::get_na_storage_method() const noexcept {
  return NaStorage::VIRTUAL;
}


size_t Virtual_ColumnImpl::get_num_data_buffers() const noexcept {
  return 0;
}


bool Virtual_ColumnImpl::is_data_editable(size_t) const {
  throw RuntimeError() << "Invalid data access for a virtual column";
}


size_t Virtual_ColumnImpl::get_data_size(size_t) const {
  throw RuntimeError() << "Invalid data access for a virtual column";
}


const void* Virtual_ColumnImpl::get_data_readonly(size_t) const {
  throw RuntimeError() << "Invalid data access for a virtual column";
}


void* Virtual_ColumnImpl::get_data_editable(size_t) {
  throw RuntimeError() << "Invalid data access for a virtual column";
}


Buffer Virtual_ColumnImpl::get_data_buffer(size_t) const {
  throw RuntimeError() << "Invalid data access for a virtual column";
}




}  // namespace dt
