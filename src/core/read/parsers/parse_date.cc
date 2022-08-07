//------------------------------------------------------------------------------
// Copyright 2021-2022 H2O.ai
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
#include <cstring>
#include "lib/hh/date.h"
#include "read/field64.h"                // field64
#include "read/parse_context.h"          // ParseContext
#include "read/parsers/info.h"
#include "utils/tests.h"
namespace dt {
namespace read {

static constexpr int32_t NA_INT32 = INT32_MIN;



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
  if (ch + 2 > eof) return false;
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


bool parse_date32_iso(const char* ch, const char* end, int32_t* out) {
  int32_t year, month, day;
  if (!parse_year(&ch, end, &year)) return false;
  if (!(ch < end && *ch == '-')) return false;
  ch++;
  if (!parse_2digits(&ch, end, &month)) return false;
  if (!(ch < end && *ch == '-')) return false;
  ch++;
  if (!parse_2digits(&ch, end, &day)) return false;
  if (!(year >= -5877641 && year <= 5879610)) return false;
  if (!(month >= 1 && month <= 12)) return false;
  if (!(day >= 1 && day <= hh::last_day_of_month(year, month))) return false;
  *out = hh::days_from_civil(year, month, day);
  return (ch == end);
}


#ifdef DT_TEST
  TEST(fread, test_date32_iso) {
    auto check = [](const char* input, int32_t expected_value, size_t advance) {
      ParseContext ctx;
      field64 output;
      ctx.ch = input;
      ctx.eof = input + std::strlen(input);
      ctx.target = &output;
      parse_date32_iso(ctx);
      ASSERT_EQ(output.int32, expected_value);
      ASSERT_EQ(ctx.ch, input + advance);
    };
    constexpr int32_t NA = NA_INT32;
    check("", NA, 0);
    check("1970-01-01", 0, 10);
    check("1970-01-011", 0, 10);
    check("1970-1-01", NA, 0);
    check("1970-01-1", NA, 0);
    check("1-11-11", -718848, 7);
    check("01-11-11", -718848, 8);
    check("001-11-11", -718848, 9);
    check("0001-11-11", -718848, 10);
    check("-001-01-14", -719880, 10);
    check("-5877641-06-24", -2147483647, 14);
    check("5879610-09-09", 2146764179, 13);
    check("2021-03-23", 18709, 10);
    check("2021-01-33", NA, 0);
    check("2021-13-13", NA, 0);
    check("2021-00-11", NA, 0);
    check("2021-01-00", NA, 0);
    check("2021-02-29", NA, 0);
  }
#endif



}}  // namespace dt::read
