//------------------------------------------------------------------------------
// Copyright 2020 H2O.ai
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
#include "column/ifelse.h"
#include "stype.h"
namespace dt {


IfElse_ColumnImpl::IfElse_ColumnImpl(
    Column&& cond, Column&& col1, Column&& col2)
  : Virtual_ColumnImpl(cond.nrows(), col1.stype()),
    cond_(std::move(cond)),
    col_true_(std::move(col1)),
    col_false_(std::move(col2))
{
  xassert(cond_.stype() == SType::BOOL);
  xassert(col_true_.stype() == col_false_.stype());
  xassert(cond_.nrows() == col_true_.nrows());
  xassert(cond_.nrows() == col_false_.nrows());
}


ColumnImpl* IfElse_ColumnImpl::clone() const {
  return new IfElse_ColumnImpl(
      Column(cond_), Column(col_true_), Column(col_false_));
}


size_t IfElse_ColumnImpl::n_children() const noexcept {
  return 3;
}

const Column& IfElse_ColumnImpl::child(size_t i) const {
  return i == 0? cond_ : i == 1? col_true_ : col_false_;
}



//------------------------------------------------------------------------------
// Element access
//------------------------------------------------------------------------------
template <typename T>
inline bool IfElse_ColumnImpl::_get(size_t i, T* out) const {
  int8_t cond_value;
  bool valid = cond_.get_element(i, &cond_value);
  if (valid) {
    valid = cond_value? col_true_.get_element(i, out)
                      : col_false_.get_element(i, out);
  }
  return valid;
}

bool IfElse_ColumnImpl::get_element(size_t i, int8_t* out)   const { return _get(i, out); }
bool IfElse_ColumnImpl::get_element(size_t i, int16_t* out)  const { return _get(i, out); }
bool IfElse_ColumnImpl::get_element(size_t i, int32_t* out)  const { return _get(i, out); }
bool IfElse_ColumnImpl::get_element(size_t i, int64_t* out)  const { return _get(i, out); }
bool IfElse_ColumnImpl::get_element(size_t i, float* out)    const { return _get(i, out); }
bool IfElse_ColumnImpl::get_element(size_t i, double* out)   const { return _get(i, out); }
bool IfElse_ColumnImpl::get_element(size_t i, CString* out)  const { return _get(i, out); }
bool IfElse_ColumnImpl::get_element(size_t i, py::oobj* out) const { return _get(i, out); }
bool IfElse_ColumnImpl::get_element(size_t i, Column* out)   const { return _get(i, out); }




}  // namespace dt
