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
#include "lib/hh/date.h"
#include "read/field64.h"                // field64
#include "read/parse_context.h"          // ParseContext
#include "read/parsers/info.h"
#include "utils/tests.h"
namespace dt {
namespace read {

static constexpr int32_t  NA_INT32 = INT32_MIN;



//------------------------------------------------------------------------------
// Date32
//------------------------------------------------------------------------------

static bool parse_year(const char** pch, const char* eof, int32_t* out) {
  const char* ch = *pch;
  if (ch == eof) return false;
  bool neg = (*ch == '-');
  ch += neg;
  if (ch + 7 < eof) eof = ch + 7;  // year can have up to 7 digits
  const char* ch0 = ch;
  int32_t value = 0;
  uint8_t digit;
  while (ch < eof && (digit = static_cast<uint8_t>(*ch - '0')) < 10) {
    value = value * 10 + static_cast<int32_t>(digit);
    ch++;
  }
  if (ch0 == ch) return false;
  if (neg) value = -value;
  *out = value;
  *pch = ch;
  return true;
}

static bool parse_2digits(const char** pch, const char* eof, int32_t* out) {
  const char* ch = *pch;
  if (ch + 2 >= eof) return false;
  uint8_t digit0 = static_cast<uint8_t>(ch[0] - '0');
  uint8_t digit1 = static_cast<uint8_t>(ch[1] - '0');
  if (digit0 < 10 && digit1 < 10) {
    *out = static_cast<int32_t>(digit0) * 10 + static_cast<int32_t>(digit1);
    *pch = ch + 2;
    return true;
  }
  return false;
}

static void parse_date32_iso(const ParseContext& ctx) {
  const char* ch = ctx.ch;
  int32_t year, month, day;
  if (!parse_year(&ch, ctx.eof, &year)) goto fail;
  if (!(ch < ctx.eof && *ch == '-')) goto fail;
  ch++;
  if (!parse_2digits(&ch, ctx.eof, &month)) goto fail;
  if (!(ch < ctx.eof && *ch == '-')) goto fail;
  ch++;
  if (!parse_2digits(&ch, ctx.eof, &day)) goto fail;
  if (!(year >= -5877641 && year <= 5879610)) goto fail;
  if (!(month >= 1 && month <= 12)) goto fail;
  if (!(day >= 1 && day <= hh::last_day_of_month(year, month))) goto fail;
  ctx.target->int32 = hh::days_from_civil(year, month, day);
  ctx.ch = ch;
  return;

  fail:
    ctx.target->int32 = NA_INT32;
}

REGISTER_PARSER(PT::Date32ISO)
    ->parser(parse_date32_iso)
    ->name("Date32/iso")
    ->code('D')
    ->type(Type::date32())
    ->successors({PT::Str32});




}}  // namespace dt::read::
