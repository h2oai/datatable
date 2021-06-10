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
#include "stype.h"
namespace dt {



//------------------------------------------------------------------------------
// CastObjToTime64_ColumnImpl
//------------------------------------------------------------------------------

CastObjToTime64_ColumnImpl::CastObjToTime64_ColumnImpl(Column&& arg)
  : Cast_ColumnImpl(SType::TIME64, std::move(arg))
{
  xassert(arg_.stype() == SType::OBJ);
}


ColumnImpl* CastObjToTime64_ColumnImpl::clone() const {
  return new CastObjToTime64_ColumnImpl(Column(arg_));
}


bool CastObjToTime64_ColumnImpl::allow_parallel_access() const {
  return false;
}


bool CastObjToTime64_ColumnImpl::get_element(size_t i, int64_t* out) const {
  py::oobj value;
  bool isvalid = arg_.get_element(i, &value);
  if (isvalid) {
    return value.parse_datetime(out) ||
           value.parse_date(out) ||
           false;
  }
  return false;
}




}  // namespace dt
