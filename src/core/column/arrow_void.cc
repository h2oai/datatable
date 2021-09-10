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
#include "column/arrow_void.h"
#include "stype.h"
namespace dt {


ArrowVoid_ColumnImpl::ArrowVoid_ColumnImpl(size_t nrows, Buffer&& valid)
  : Arrow_ColumnImpl(nrows, SType::VOID),
    validity_(std::move(valid)) {}


ColumnImpl* ArrowVoid_ColumnImpl::clone() const {
  return new ArrowVoid_ColumnImpl(nrows_, Buffer(validity_));
}


size_t ArrowVoid_ColumnImpl::get_num_data_buffers() const noexcept {
  return 1;
}

Buffer ArrowVoid_ColumnImpl::get_data_buffer(size_t) const {
  return validity_;
}


bool ArrowVoid_ColumnImpl::get_element(size_t, int8_t*) const {
  return false;
}




}  // namespace dt
