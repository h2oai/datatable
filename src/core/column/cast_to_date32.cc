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
#include "read/parsers/info.h"
#include "stype.h"
namespace dt {



//------------------------------------------------------------------------------
// CastTime64ToDate32_ColumnImpl
//------------------------------------------------------------------------------

CastTime64ToDate32_ColumnImpl::CastTime64ToDate32_ColumnImpl(Column&& arg)
  : Cast_ColumnImpl(SType::DATE32, std::move(arg))
{
  xassert(arg_.can_be_read_as<int64_t>());
}


ColumnImpl* CastTime64ToDate32_ColumnImpl::clone() const {
  return new CastTime64ToDate32_ColumnImpl(Column(arg_));
}


bool CastTime64ToDate32_ColumnImpl::get_element(size_t i, int32_t* out) const {
  int64_t value;
  bool isvalid = arg_.get_element(i, &value);
  if (isvalid) {
    constexpr int64_t NANOS_IN_DAY = 24ll * 3600ll * 1000000000ll;
    if (value < 0) {
      value -= NANOS_IN_DAY - 1;
    }
    *out = static_cast<int32_t>(value / NANOS_IN_DAY);
  }
  return isvalid;
}




//------------------------------------------------------------------------------
// CastStringToDate32_ColumnImpl
//------------------------------------------------------------------------------

CastStringToDate32_ColumnImpl::CastStringToDate32_ColumnImpl(Column&& arg)
  : Cast_ColumnImpl(SType::DATE32, std::move(arg))
{
  xassert(arg_.can_be_read_as<CString>());
}


ColumnImpl* CastStringToDate32_ColumnImpl::clone() const {
  return new CastStringToDate32_ColumnImpl(Column(arg_));
}


bool CastStringToDate32_ColumnImpl::get_element(size_t i, int32_t* out) const {
  CString value;
  bool isvalid = arg_.get_element(i, &value);
  return isvalid &&
         read::parse_date32_iso(value.data(), value.end(), out);
}





//------------------------------------------------------------------------------
// CastObjToDate32_ColumnImpl
//------------------------------------------------------------------------------

CastObjToDate32_ColumnImpl::CastObjToDate32_ColumnImpl(Column&& arg)
  : Cast_ColumnImpl(SType::DATE32, std::move(arg))
{
  xassert(arg_.stype() == SType::OBJ);
}


ColumnImpl* CastObjToDate32_ColumnImpl::clone() const {
  return new CastObjToDate32_ColumnImpl(Column(arg_));
}


bool CastObjToDate32_ColumnImpl::allow_parallel_access() const {
  return false;
}


bool CastObjToDate32_ColumnImpl::get_element(size_t i, int32_t* out) const {
  py::oobj value;
  bool isvalid = arg_.get_element(i, &value);
  if (isvalid) {
    return value.parse_date_as_date(out) ||
           value.parse_int_as_date(out) ||
           value.parse_datetime_as_date(out) ||
           value.parse_string_as_date(out);
  }
  return false;
}





}  // namespace dt
