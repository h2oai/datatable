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
#include "column/cast.h"
#include "stype.h"
namespace dt {


//------------------------------------------------------------------------------
// CastNumericToBool_ColumnImpl
//------------------------------------------------------------------------------

template <typename T>
CastNumericToBool_ColumnImpl<T>::CastNumericToBool_ColumnImpl(Column&& arg)
  : Cast_ColumnImpl(SType::BOOL, std::move(arg))
{ xassert(arg_.can_be_read_as<T>()); }


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




//------------------------------------------------------------------------------
// CastStringToBool_ColumnImpl
//------------------------------------------------------------------------------

CastStringToBool_ColumnImpl::CastStringToBool_ColumnImpl(Column&& arg)
  : Cast_ColumnImpl(SType::BOOL, std::move(arg))
{ xassert(arg_.can_be_read_as<CString>()); }


ColumnImpl* CastStringToBool_ColumnImpl::clone() const {
  return new CastStringToBool_ColumnImpl(Column(arg_));
}


bool CastStringToBool_ColumnImpl::get_element(size_t i, int8_t* out) const {
  static CString str_true ("True", 4);
  static CString str_false("False", 5);
  CString value;
  bool isvalid = arg_.get_element(i, &value);
  if (isvalid) {
    if (value == str_true)       *out = 1;
    else if (value == str_false) *out = 0;
    else                         return false;
  }
  return isvalid;
}




//------------------------------------------------------------------------------
// CastObjToBool_ColumnImpl
//------------------------------------------------------------------------------

CastObjToBool_ColumnImpl::CastObjToBool_ColumnImpl(Column&& arg)
  : Cast_ColumnImpl(SType::BOOL, std::move(arg))
{ xassert(arg_.stype() == SType::OBJ); }


ColumnImpl* CastObjToBool_ColumnImpl::clone() const {
  return new CastObjToBool_ColumnImpl(Column(arg_));
}


bool CastObjToBool_ColumnImpl::allow_parallel_access() const {
  return false;
}


bool CastObjToBool_ColumnImpl::get_element(size_t i, int8_t* out) const {
  py::oobj value;
  bool isvalid = arg_.get_element(i, &value);
  if (isvalid) {
    *out = value.to_bool_force();
    if (*out == -128) isvalid = false;
  }
  return isvalid;
}




}  // namespace dt
