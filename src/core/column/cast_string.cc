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
namespace dt {



static bool parse_int(const char* ch, const char* end, int64_t* out) {
  if (ch == end) return false;
  bool negative = (*ch == '-');
  ch += negative || (*ch == '+');       // skip sign
  if (ch == end) return false;

  uint64_t value = 0;
  for (; ch < end; ch++) {
    auto digit = static_cast<uint8_t>(*ch - '0');
    if (digit >= 10) return false;
    value = 10*value + digit;
  }

  *out = negative? -static_cast<int64_t>(value) : static_cast<int64_t>(value);
  return true;
}


template <typename V>
inline bool CastString_ColumnImpl::_parse_int(size_t i, V* out) const {
  CString x;
  bool isvalid = arg_.get_element(i, &x);
  if (isvalid) {
    int64_t y;
    isvalid = parse_int(x.data(), x.end(), &y);
    *out = static_cast<V>(y);
  }
  return isvalid;
}



ColumnImpl* CastString_ColumnImpl::clone() const {
  return new CastString_ColumnImpl(stype_, Column(arg_));
}





bool CastString_ColumnImpl::get_element(size_t i, int8_t* out) const {
  return _parse_int<int8_t>(i, out);
}


bool CastString_ColumnImpl::get_element(size_t i, int16_t* out) const {
  return _parse_int<int16_t>(i, out);
}


bool CastString_ColumnImpl::get_element(size_t i, int32_t* out) const {
  return _parse_int<int32_t>(i, out);
}


bool CastString_ColumnImpl::get_element(size_t i, int64_t* out) const {
  return _parse_int<int64_t>(i, out);
}


/*

bool CastString_ColumnImpl::get_element(size_t i, float* out) const {
  return _get<float>(i, out);
}


bool CastString_ColumnImpl::get_element(size_t i, double* out) const {
  return _get<double>(i, out);
}


bool CastString_ColumnImpl::get_element(size_t i, CString* out)  const {
  int8_t x;
  bool isvalid = arg_.get_element(i, &x);
  if (isvalid) {
    if (x) out->set("True", 4);
    else   out->set("False", 5);
  }
  return isvalid;
}


bool CastString_ColumnImpl::get_element(size_t i, py::oobj* out) const {
  static py::oobj obj_true = py::True();
  static py::oobj obj_false = py::False();
  int8_t x;
  bool isvalid = arg_.get_element(i, &x);
  if (isvalid) {
    *out = x? obj_true : obj_false;
  }
  return isvalid;
}


template <typename T>
inline bool CastString_ColumnImpl::_get(size_t i, T* out) const {
  int8_t x;
  bool isvalid = arg_.get_element(i, &x);
  *out = static_cast<T>(x);
  return isvalid;
}
*/



}  // namespace dt
