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


ColumnImpl* NpMasked_ColumnImpl::clone() const {
  auto res = new NpMasked_ColumnImpl(Column(arg_), Buffer(mask_));
  res->nrows_ = nrows_;
  return res;
}



template <typename T>
void NpMasked_ColumnImpl::_apply_mask(Column& out) {
  assert_compatible_type<T>(arg_.stype());
  auto mask_data = static_cast<const bool*>(mask_.rptr());
  auto col_data = static_cast<T*>(arg_.get_data_editable());

  parallel_for_static(nrows_,
    [=](size_t i) {
      if (mask_data[i]) col_data[i] = GETNA<T>();
    });
  out = std::move(arg_);
}

void NpMasked_ColumnImpl::materialize(Column& out, bool to_memory) {
  if (arg_.get_na_storage_method() == NaStorage::SENTINEL &&
      arg_.is_fixedwidth() &&
      arg_.is_data_editable())
  {
    switch (stype_) {
      case SType::BOOL:
      case SType::INT8:    return _apply_mask<int8_t>(out);
      case SType::INT16:   return _apply_mask<int16_t>(out);
      case SType::INT32:   return _apply_mask<int32_t>(out);
      case SType::INT64:   return _apply_mask<int64_t>(out);
      case SType::FLOAT32: return _apply_mask<float>(out);
      case SType::FLOAT64: return _apply_mask<double>(out);
      default: break;
    }
  }
  ColumnImpl::materialize(out, to_memory);
}



//------------------------------------------------------------------------------
// Element access
//------------------------------------------------------------------------------

template <typename T>
inline bool NpMasked_ColumnImpl::_get(size_t i, T* out) const {
  if (static_cast<const bool*>(mask_.rptr())[i]) return false;
  return arg_.get_element(i, out);
}

bool NpMasked_ColumnImpl::get_element(size_t i, int8_t* out)   const { return _get(i, out); }
bool NpMasked_ColumnImpl::get_element(size_t i, int16_t* out)  const { return _get(i, out); }
bool NpMasked_ColumnImpl::get_element(size_t i, int32_t* out)  const { return _get(i, out); }
bool NpMasked_ColumnImpl::get_element(size_t i, int64_t* out)  const { return _get(i, out); }
bool NpMasked_ColumnImpl::get_element(size_t i, float* out)    const { return _get(i, out); }
bool NpMasked_ColumnImpl::get_element(size_t i, double* out)   const { return _get(i, out); }
bool NpMasked_ColumnImpl::get_element(size_t i, CString* out)  const { return _get(i, out); }
bool NpMasked_ColumnImpl::get_element(size_t i, py::robj* out) const { return _get(i, out); }




}  // namespace dt
