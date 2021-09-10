//------------------------------------------------------------------------------
// Copyright 2020-2021 H2O.ai
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
#include "column/ifelsen.h"
#include "stype.h"
namespace dt {


IfElseN_ColumnImpl::IfElseN_ColumnImpl(colvec&& cond, colvec&& vals)
  : Virtual_ColumnImpl(vals[0].nrows(), vals[0].stype()),
    conditions_(std::move(cond)),
    values_(std::move(vals))
{
  #if DT_DEBUG
    xassert(conditions_.size() == values_.size() - 1);
    for (const Column& cnd : conditions_) {
      xassert(cnd.stype() == SType::BOOL);
      xassert(cnd.nrows() == nrows_);
    }
    for (const Column& val : values_) {
      xassert(val.stype() == stype());
      xassert(val.nrows() == nrows());
    }
  #endif
}


ColumnImpl* IfElseN_ColumnImpl::clone() const {
  return new IfElseN_ColumnImpl(colvec(conditions_), colvec(values_));
}


size_t IfElseN_ColumnImpl::n_children() const noexcept {
  return conditions_.size() + values_.size();
}

const Column& IfElseN_ColumnImpl::child(size_t i) const {
  return i < conditions_.size()? conditions_[i]
                               : values_[i - conditions_.size()];
}



//------------------------------------------------------------------------------
// Element access
//------------------------------------------------------------------------------
template <typename T>
inline bool IfElseN_ColumnImpl::_get(size_t i, T* out) const {
  for (size_t j = 0; j < conditions_.size(); ++j) {
    int8_t condition_value;
    bool valid = conditions_[j].get_element(i, &condition_value);
    if (!valid) return false;
    if (condition_value) return values_[j].get_element(i, out);
  }
  return values_.back().get_element(i, out);
}

bool IfElseN_ColumnImpl::get_element(size_t i, int8_t* out)   const { return _get(i, out); }
bool IfElseN_ColumnImpl::get_element(size_t i, int16_t* out)  const { return _get(i, out); }
bool IfElseN_ColumnImpl::get_element(size_t i, int32_t* out)  const { return _get(i, out); }
bool IfElseN_ColumnImpl::get_element(size_t i, int64_t* out)  const { return _get(i, out); }
bool IfElseN_ColumnImpl::get_element(size_t i, float* out)    const { return _get(i, out); }
bool IfElseN_ColumnImpl::get_element(size_t i, double* out)   const { return _get(i, out); }
bool IfElseN_ColumnImpl::get_element(size_t i, CString* out)  const { return _get(i, out); }
bool IfElseN_ColumnImpl::get_element(size_t i, py::oobj* out) const { return _get(i, out); }
bool IfElseN_ColumnImpl::get_element(size_t i, Column* out)   const { return _get(i, out); }




}  // namespace dt
