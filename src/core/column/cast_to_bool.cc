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
#include "column/cast.h"
#include "stype.h"
namespace dt {


template <typename T>
CastNumericToBool_ColumnImpl<T>::CastNumericToBool_ColumnImpl(Column&& arg)
  : Cast_ColumnImpl(SType::BOOL, std::move(arg)) {}


template <typename T>
ColumnImpl* CastNumericToBool_ColumnImpl<T>::clone() const {
  return new CastNumericToBool_ColumnImpl<T>(Column(arg_));
}


template <typename T>
bool CastNumericToBool_ColumnImpl<T>::get_element(size_t i, int8_t* out) const {
  T value;
  bool isvalid = arg_.get_element(i, &value);
  *out = (value != 0);
  return isvalid;
}



template class CastNumericToBool_ColumnImpl<int8_t>;
template class CastNumericToBool_ColumnImpl<int16_t>;
template class CastNumericToBool_ColumnImpl<int32_t>;
template class CastNumericToBool_ColumnImpl<int64_t>;
template class CastNumericToBool_ColumnImpl<float>;
template class CastNumericToBool_ColumnImpl<double>;



}  // namespace dt
