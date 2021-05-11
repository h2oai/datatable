//------------------------------------------------------------------------------
// Copyright 2018-2021 H2O.ai
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
#include <limits>
#include "read/field64.h"                // field64
#include "read/parse_context.h"          // ParseContext
#include "read/parsers/info.h"
namespace dt {
namespace read {

static constexpr int32_t NA_INT32 = INT32_MIN;
static constexpr int64_t NA_INT64 = INT64_MIN;



//------------------------------------------------------------------------------
// Regular integers
//------------------------------------------------------------------------------

template <typename T, bool allow_leading_zeroes>
void parse_int_simple(const ParseContext& ctx) {
  constexpr int MAX_DIGITS = sizeof(T) == 4? 10 : 19;
  constexpr uint64_t MAX_VALUE = std::numeric_limits<T>::max();
  constexpr T NA_VALUE = std::numeric_limits<T>::min();
  const char* ch = ctx.ch;
  bool negative = ch < ctx.eof && *ch == '-';

  ch += negative || (ch < ctx.eof && *ch == '+');
  const char* start = ch;     // to check if at least one digit is present
  uint64_t value = 0;         // value accumulator
  uint8_t  digit;             // current digit being read
  int sd = 0;                 // number of significant digits (without initial 0s)

  if (allow_leading_zeroes) {
    while (ch < ctx.eof && *ch == '0') ch++;  // skip leading zeros
  } else if (ch < ctx.eof && *ch == '0') {
    *reinterpret_cast<T*>(ctx.target) = 0;
    ctx.ch = ch + 1;
    return;
  }
  while (ch + sd < ctx.eof && (digit = static_cast<uint8_t>(ch[sd] - '0')) < 10) {
    value = 10*value + digit;
    sd++;
  }
  ch += sd;
  // Usually `0 < sd < MAX_DIGITS`, and no other checks are needed.
  // If `sd == 0` then the input is valid iff it is "0" (or multiple 0s,
  // possibly with a sign), which can be checked via `ch > start`.
  // If `sd == MAX_DIGITS`, then we need to check that the value did not
  // overflow. Since the accumulator is uint64_t, it can hold integer
  // values up to 18446744073709551615. This is enough to fit any 10- or
  // 19-digit number (even 10**19 - 1).
  if ((sd > 0 && sd < MAX_DIGITS) ||
      (sd == 0 && ch > start) ||
      (sd == MAX_DIGITS && value <= MAX_VALUE))
  {
    T x = static_cast<T>(value);
    T neg = static_cast<T>(negative);
    *reinterpret_cast<T*>(ctx.target) = (x ^ -neg) + neg; // same as neg? -x : x
    ctx.ch = ch;
  } else {
    *reinterpret_cast<T*>(ctx.target) = NA_VALUE;
  }
}

REGISTER_PARSER(PT::Int32)
    ->parser(parse_int_simple<int32_t, true>)
    ->name("Int32")
    ->code('i')
    ->type(Type::int32())
    ->successors({PT::Int32Sep, PT::Int64, PT::Int64Sep,
                  PT::Float64Plain, PT::Float64Ext, PT::Str32});

REGISTER_PARSER(PT::Int64)
    ->parser(parse_int_simple<int64_t, true>)
    ->name("Int64")
    ->code('I')
    ->type(Type::int64())
    ->successors({PT::Int64Sep, PT::Float64Plain, PT::Float64Ext, PT::Str32});




//------------------------------------------------------------------------------
// Parse integers where thousands are separated into groups, eg
//   1,000,000
//     100,000
//          17
//       00001  // output of `printf("%'05d", 1)` -- initial zeros are not
//              // comma-separated
//
// `T` should be either int32_t or int64_t
//------------------------------------------------------------------------------

template <typename T>
void parse_int_grouped(const ParseContext& ctx) {
  const char* ch = ctx.ch;
  bool quoted = ch < ctx.eof && *ch == ctx.quote;
  ch += quoted;
  bool negative = ch < ctx.eof && *ch == '-';
  ch += (negative || (ch < ctx.eof && *ch == '+'));

  const char thsep = quoted || ctx.sep != ','? ',' : '\xFF';
  const char* start = ch;  // to check if at least one digit is present
  uint_fast64_t acc = 0;   // value accumulator
  uint8_t  digit;          // current digit being read
  int sf = 0;              // number of significant digits (without initial 0s)
  int gr = 0;              // number of digits in the current digit group

  while (ch < ctx.eof && *ch == '0') ch++;   // skip leading zeros
  while (ch < ctx.eof && (digit = static_cast<uint8_t>(*ch - '0')) < 10) {
    acc = 10*acc + digit;
    ch++;
    sf++;
    gr++;
    if (ch < ctx.eof && *ch == thsep) {
      if (gr > 3 || (gr < 3 && gr != sf)) goto fail;
      gr = 0;  // restart the digit group
      ch++;    // skip over the thousands separator
    }
  }
  // Check that the last group has the correct number of digits (or a number
  // without any thousand separators should be considered valid too).
  if (gr != 3 && gr != sf) goto fail;
  // Check that a quoted field properly ends with a quote
  if (quoted) {
    if ((ch < ctx.eof && *ch != ctx.quote) || ch == ctx.eof) goto fail;
    ch++;
  } else {
    // Make sure we do not confuse field separator with thousands separator when
    // the field isn't quoted.
    if (gr != sf && thsep == '\xFF') goto fail;
  }

  // Usually `0 < sf < max_digits`, and the condition short-circuits.
  // If `sf == 0` then the input is valid iff it is "0" (or multiple 0s,
  // possibly with a sign), which can be checked via `ch > start`.
  // If `sf == max_digits`, then we explicitly check for overflow against
  // `max_value` (noting that `uint64_t` can hold values up to
  // 18446744073709551615, which is sufficient to represent any 19-digit
  // number up to 9999999999999999999).
  static constexpr int max_digits = sizeof(T) == sizeof(int32_t)? 10 : 19;
  static constexpr T max_value = std::numeric_limits<T>::max();
  if ((sf? sf < max_digits : ch > start) ||
      (sf == max_digits && acc <= max_value))
  {
    T x = static_cast<T>(acc);

    #if DT_COMPILER_MSVC
      #pragma warning(push)
      // conversion from 'T' to 'int32_t', possible loss of data
      #pragma warning(disable : 4244)
    #endif

    if (sizeof(T) == 4) ctx.target->int32 = negative? -x : x;
    if (sizeof(T) == 8) ctx.target->int64 = negative? -x : x;

    #if DT_COMPILER_MSVC
      #pragma warning(pop)
    #endif

    ctx.ch = ch;
    return;
  }

  fail:
    if (sizeof(T) == 4) ctx.target->int32 = NA_INT32;
    if (sizeof(T) == 8) ctx.target->int64 = NA_INT64;
}

REGISTER_PARSER(PT::Int32Sep)
    ->parser(parse_int_grouped<int32_t>)
    ->name("Int32/grouped")
    ->code('i')
    ->type(Type::int32())
    ->successors({PT::Int64Sep, PT::Str32});

REGISTER_PARSER(PT::Int64Sep)
    ->parser(parse_int_grouped<int64_t>)
    ->name("Int64/grouped")
    ->code('I')
    ->type(Type::int64())
    ->successors({PT::Str32});




}}  // namespace dt::read::
