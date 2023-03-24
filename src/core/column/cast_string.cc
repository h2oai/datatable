//------------------------------------------------------------------------------
// Copyright 2020-2023 H2O.ai
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
#include <cmath>             // std::ismath
#include <cstdlib>           // std::strtod
#include "column/cast.h"
#include "python/string.h"
#include "read/constants.h"  // dt::read::pow10lookup
namespace dt {



ColumnImpl* CastString_ColumnImpl::clone() const {
  return new CastString_ColumnImpl(stype(), Column(arg_));
}



//------------------------------------------------------------------------------
// Parse integers
//------------------------------------------------------------------------------

static bool parse_int(const char* ch, const char* end, int64_t* out) {
  if (ch == end) return false;
  bool negative = (*ch == '-');
  ch += negative || (*ch == '+');  // skip sign
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
inline bool CastString_ColumnImpl::_get_int(size_t i, V* out) const {
  CString x;
  bool isvalid = arg_.get_element(i, &x);
  if (isvalid) {
    int64_t y = 0;
    isvalid = parse_int(x.data(), x.end(), &y);
    *out = static_cast<V>(y);
  }
  return isvalid;
}


bool CastString_ColumnImpl::get_element(size_t i, int8_t* out) const {
  return _get_int<int8_t>(i, out);
}


bool CastString_ColumnImpl::get_element(size_t i, int16_t* out) const {
  return _get_int<int16_t>(i, out);
}


bool CastString_ColumnImpl::get_element(size_t i, int32_t* out) const {
  return _get_int<int32_t>(i, out);
}


bool CastString_ColumnImpl::get_element(size_t i, int64_t* out) const {
  return _get_int<int64_t>(i, out);
}




//------------------------------------------------------------------------------
// Parse floats
//------------------------------------------------------------------------------

static bool parse_double(const char* ch, const char* end, double* out) {
  constexpr int MAX_DIGITS = 18;
  if (ch == end) return false;
  bool negative = (*ch == '-');
  ch += negative || (*ch == '+');  // skip sign
  if (ch == end) return false;

  const char* start = ch; // beginning of the number, without the initial sign
  uint64_t mantissa = 0;  // mantissa NNN.MMM as a single 64-bit integer NNNMMM
  int_fast32_t e = 0;     // the number's exponent. The value being parsed is
                          // equal to mantissa·10ᵉ
  uint_fast8_t digit;     // temporary variable, holds last scanned digit.

  // Skip leading zeros
  while (ch < end && *ch == '0') ch++;

  // Read the first, integer part of the floating number (but no more than
  // MAX_DIGITS digits).
  int_fast32_t sflimit = MAX_DIGITS;
  while (ch < end && (digit = static_cast<uint_fast8_t>(*ch - '0')) < 10 && sflimit) {
    mantissa = 10*mantissa + digit;
    sflimit--;
    ch++;
  }

  // If maximum allowed number of digits were read, but more are present -- then
  // we will read and discard those extra digits, but only if they are followed
  // by a decimal point (otherwise it's a just big integer, which should be
  // treated as a string instead of losing precision).
  if (ch < end && sflimit == 0 && static_cast<uint_fast8_t>(*ch - '0') < 10) {
    while (ch < end && static_cast<uint_fast8_t>(*ch - '0') < 10) {
      ch++;
      e++;
    }
    if (ch == end || *ch != '.') return false;
  }

  // Read the fractional part of the number, if it's present
  if (ch < end && *ch == '.') {
    ch++;  // skip the dot
    // If the integer part was 0, then leading zeros in the fractional part do
    // not count against the number's precision: skip them.
    if (ch < end && *ch == '0' && mantissa == 0) {
      int_fast32_t k = 0;
      while (ch + k < end && ch[k] == '0') k++;
      ch += k;
      e = -k;
    }
    // Now read the significant digits in the fractional part of the number
    int_fast32_t k = 0;
    while (ch + k < end && (digit = static_cast<uint_fast8_t>(ch[k] - '0')) < 10 && sflimit) {
      mantissa = 10*mantissa + digit;
      k++;
      sflimit--;
    }
    ch += k;
    e -= k;
    // If more digits are present, skip them
    if (ch < end && sflimit == 0 && static_cast<uint_fast8_t>(*ch - '0') < 10) {
      ch++;
      while (ch < end && static_cast<uint_fast8_t>(*ch - '0') < 10) ch++;
    }
    // Check that at least 1 digit was present either in the integer or
    // fractional part ("+1" here accounts for the decimal point symbol).
    if (ch == start + 1) return false;
  }
  // If there is no fractional part, then check that the integer part actually
  // exists (otherwise it's not a valid number)...
  else {
    if (ch == start) return false;
  }

  // Now scan the "exponent" part of the number (if present)
  if (ch < end && (*ch == 'E' || *ch == 'e')) {
    bool Eneg = 0;
    if (ch + 1 < end) {
      ch += (Eneg = (ch[1]=='-')) + (ch[1]=='+');
    }
    ch += 1;

    int_fast32_t exp = 0;
    if (ch < end && (digit = static_cast<uint_fast8_t>(*ch - '0')) < 10) {
      exp = digit;
      ch++;
      if (ch < end && (digit = static_cast<uint_fast8_t>(*ch - '0')) < 10) {
        exp = exp*10 + digit;
        ch++;
        if (ch < end && (digit = static_cast<uint_fast8_t>(*ch - '0')) < 10) {
          exp = exp*10 + digit;
          ch++;
        }
      }
    } else {
      return false;
    }
    e += Eneg? -exp : exp;
  }

  if (e < -350 || e > 350 || ch != end) return false;
  auto r = static_cast<long double>(mantissa);

  // Handling of very small and very large floats.
  // Based on https://github.com/Rdatatable/data.table/pull/4165
  if (e < -300 || e > 300) {
    auto extra = static_cast<int_fast8_t>(e - copysign(300, e));
    r *= dt::read::pow10lookup[extra + 300];
    e -= extra;
  }
  e += 300;
  r *= dt::read::pow10lookup[e];
  *out = static_cast<double>(negative? -r : r);

  return true;
}


template <typename V>
inline bool CastString_ColumnImpl::_get_float(size_t i, V* out) const {
  CString x;
  bool isvalid = arg_.get_element(i, &x);
  if (isvalid) {
    double y;
    isvalid = parse_double(x.data(), x.end(), &y);
    *out = static_cast<V>(y);
  }
  return isvalid;
}


bool CastString_ColumnImpl::get_element(size_t i, float* out) const {
  return _get_float<float>(i, out);
}


bool CastString_ColumnImpl::get_element(size_t i, double* out) const {
  return _get_float<double>(i, out);
}



//------------------------------------------------------------------------------
// Parse other
//------------------------------------------------------------------------------

bool CastString_ColumnImpl::get_element(size_t i, CString* out) const {
  return arg_.get_element(i, out);
}


bool CastString_ColumnImpl::get_element(size_t i, py::oobj* out) const {
  CString x;
  bool isvalid = arg_.get_element(i, &x);
  if (isvalid) {
    *out = py::ostring(x);
  }
  return isvalid;
}




}  // namespace dt
