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
#include "column/npmasked.h"
#include "parallel/api.h"
namespace dt {



NpMasked_ColumnImpl::NpMasked_ColumnImpl(Column&& arg, Buffer&& mask)
  : Virtual_ColumnImpl(arg.nrows(), arg.stype()),
    arg_(std::move(arg)),
    mask_(std::move(mask))
{
  XAssert(arg_.nrows() == mask_.size());
}


ColumnImpl* NpMasked_ColumnImpl::shallowcopy() const {
  return new NpMasked_ColumnImpl(Column(arg_), Buffer(mask_));
}


template <typename T>
static void _apply_mask(T* data, const bool* mask, size_t nrows) {
  parallel_for_static(nrows,
    [=](size_t i) {
      if (mask[i]) data[i] = GETNA<T>();
    });
}

ColumnImpl* NpMasked_ColumnImpl::materialize() {
  if (!arg_.is_virtual() && arg_.is_fixedwidth() && arg_.is_data_editable()) {
    auto mask_data = static_cast<const bool*>(mask_.rptr());
    void* arg_data = arg_.get_data_editable();
    switch (stype_) {
      case SType::BOOL:
      case SType::INT8:    _apply_mask(static_cast<int8_t*>(arg_data), mask_data, nrows_); break;
      case SType::INT16:   _apply_mask(static_cast<int16_t*>(arg_data), mask_data, nrows_); break;
      case SType::INT32:   _apply_mask(static_cast<int32_t*>(arg_data), mask_data, nrows_); break;
      case SType::INT64:   _apply_mask(static_cast<int64_t*>(arg_data), mask_data, nrows_); break;
      case SType::FLOAT32: _apply_mask(static_cast<float*>(arg_data), mask_data, nrows_); break;
      case SType::FLOAT64: _apply_mask(static_cast<double*>(arg_data), mask_data, nrows_); break;
      default: return ColumnImpl::materialize();
    }
    return arg_.release();
  }
  else {
    return ColumnImpl::materialize();
  }
}



//------------------------------------------------------------------------------
// Element access
//------------------------------------------------------------------------------

bool NpMasked_ColumnImpl::get_element(size_t i, int8_t* out)  const {
  if (static_cast<const bool*>(mask_.rptr())[i]) return false;
  return arg_.get_element(i, out);
}


bool NpMasked_ColumnImpl::get_element(size_t i, int16_t* out) const {
  if (static_cast<const bool*>(mask_.rptr())[i]) return false;
  return arg_.get_element(i, out);
}


bool NpMasked_ColumnImpl::get_element(size_t i, int32_t* out) const {
  if (static_cast<const bool*>(mask_.rptr())[i]) return false;
  return arg_.get_element(i, out);
}


bool NpMasked_ColumnImpl::get_element(size_t i, int64_t* out) const {
  if (static_cast<const bool*>(mask_.rptr())[i]) return false;
  return arg_.get_element(i, out);
}


bool NpMasked_ColumnImpl::get_element(size_t i, float* out)   const {
  if (static_cast<const bool*>(mask_.rptr())[i]) return false;
  return arg_.get_element(i, out);
}


bool NpMasked_ColumnImpl::get_element(size_t i, double* out)  const {
  if (static_cast<const bool*>(mask_.rptr())[i]) return false;
  return arg_.get_element(i, out);
}


bool NpMasked_ColumnImpl::get_element(size_t i, CString* out) const {
  if (static_cast<const bool*>(mask_.rptr())[i]) return false;
  return arg_.get_element(i, out);
}


bool NpMasked_ColumnImpl::get_element(size_t i, py::robj* out) const {
  if (static_cast<const bool*>(mask_.rptr())[i]) return false;
  return arg_.get_element(i, out);
}





}  // namespace dt
