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
#include <cstddef>           // offsetof
#include "column/cast.h"
#include "csv/reader_parsers.h"
#include "read/parse_context.h"
namespace dt {


struct MiniParseContext {
  const char* ch;
  const char* eof;
  read::field64* target;
};
static_assert(offsetof(MiniParseContext, ch) == offsetof(read::ParseContext, ch), "Invalid offset of MiniParseContext::ch");
static_assert(offsetof(MiniParseContext, eof) == offsetof(read::ParseContext, eof), "Invalid offset of MiniParseContext::eof");
static_assert(offsetof(MiniParseContext, target) == offsetof(read::ParseContext, target), "Invalid offset of MiniParseContext::target");

template <typename V>
inline bool CastString_ColumnImpl::_parse_int(size_t i, V* out) const {
  using M = typename std::conditional<std::is_same<V, int64_t>::value,
                                      int64_t, int32_t>::type;
  CString x;
  bool isvalid = arg_.get_element(i, &x);
  if (isvalid) {
    const char* ch = x.data();
    const char* end = x.end();
    M y;
    MiniParseContext ctx { ch, end, reinterpret_cast<read::field64*>(&y) };
    parse_int_simple<M, true>(*reinterpret_cast<read::ParseContext*>(&ctx));
    if (ctx.ch == end) {
      *out = static_cast<V>(y);
      return true;
    }
  }
  return false;
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
