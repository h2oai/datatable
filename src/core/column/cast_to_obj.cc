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
#include "python/datetime.h"
#include "python/list.h"
#include "read/parsers/info.h"
#include "stype.h"
namespace dt {



//------------------------------------------------------------------------------
// CastTime64ToObj64_ColumnImpl
//------------------------------------------------------------------------------

CastTime64ToObj64_ColumnImpl::CastTime64ToObj64_ColumnImpl(Column&& arg)
  : Cast_ColumnImpl(SType::OBJ, std::move(arg))
{
  xassert(arg_.can_be_read_as<int64_t>());
}


ColumnImpl* CastTime64ToObj64_ColumnImpl::clone() const {
  return new CastTime64ToObj64_ColumnImpl(Column(arg_));
}


bool CastTime64ToObj64_ColumnImpl::get_element(size_t i, py::oobj* out) const {
  int64_t value;
  bool isvalid = arg_.get_element(i, &value);
  if (isvalid) {
    *out = py::odatetime(value);
  }
  return isvalid;
}



//------------------------------------------------------------------------------
// CastArrayToObject_ColumnImpl
//------------------------------------------------------------------------------

CastArrayToObject_ColumnImpl::CastArrayToObject_ColumnImpl(Column&& arg)
  : Cast_ColumnImpl(SType::OBJ, std::move(arg))
{
  xassert(arg_.can_be_read_as<Column>());
}


ColumnImpl* CastArrayToObject_ColumnImpl::clone() const {
  return new CastArrayToObject_ColumnImpl(Column(arg_));
}


bool CastArrayToObject_ColumnImpl::get_element(size_t i, py::oobj* out) const {
  Column value;
  bool isvalid = arg_.get_element(i, &value);
  if (isvalid) {
    value.cast_inplace(dt::Type::obj64());
    auto n = value.nrows();
    auto res = py::olist(n);
    py::oobj item;
    for (size_t j = 0; j < n; j++) {
      bool item_isvalid = value.get_element(j, &item);
      res.set(j, item_isvalid? item : py::None());
    }
    *out = std::move(res);
  }
  return isvalid;
}




}  // namespace dt
