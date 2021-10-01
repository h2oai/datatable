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
#include "column/cast.h"
#include "python/list.h"
#include "read/parsers/info.h"
#include "stype.h"
namespace dt {



//------------------------------------------------------------------------------
// CastArrayToArray_ColumnImpl
//------------------------------------------------------------------------------

CastArrayToArray_ColumnImpl::CastArrayToArray_ColumnImpl(
    Column&& arg, Type targetType)
  : Cast_ColumnImpl(targetType, std::move(arg)),
    childType_(targetType.child())
{
  xassert(arg_.type().is_array());
}


ColumnImpl* CastArrayToArray_ColumnImpl::clone() const {
  return new CastArrayToArray_ColumnImpl(Column(arg_), type());
}


bool CastArrayToArray_ColumnImpl::get_element(size_t i, Column* out) const {
  bool isvalid = arg_.get_element(i, out);
  if (isvalid) {
    out->cast_inplace(childType_);
  }
  return isvalid;
}



//------------------------------------------------------------------------------
// CastObjectToArray_ColumnImpl
//------------------------------------------------------------------------------

CastObjectToArray_ColumnImpl::CastObjectToArray_ColumnImpl(
    Column&& arg, Type targetType)
  : Cast_ColumnImpl(targetType, std::move(arg)),
    childType_(targetType.child())
{
  xassert(arg_.type().is_object());
}


ColumnImpl* CastObjectToArray_ColumnImpl::clone() const {
  return new CastObjectToArray_ColumnImpl(Column(arg_), type());
}


bool CastObjectToArray_ColumnImpl::get_element(size_t i, Column* out) const {
  py::oobj value;
  bool isvalid = arg_.get_element(i, &value);
  if (isvalid && value.is_list_or_tuple()) {
    *out = Column::from_pylist(value.to_pylist(), childType_);
    return true;
  }
  return false;
}




}  // namespace dt
