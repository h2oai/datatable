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
#include <cstring>
#include "lib/hh/date.h"
#include "read/field64.h"                // field64
#include "read/parse_context.h"          // ParseContext
#include "read/parsers/info.h"
#include "utils/tests.h"
namespace dt {
namespace read {

static constexpr int64_t NA_INT64 = INT64_MIN;
static constexpr int64_t NANOSECONDS_PER_SECOND = 1000000000LL;
static constexpr int64_t NANOSECONDS_PER_DAY = 1000000000LL * 24LL * 3600LL;


//------------------------------------------------------------------------------
// Time64
//------------------------------------------------------------------------------

static bool parse_4digits(const char* ch, int32_t* out) {
  uint8_t digit0 = static_cast<uint8_t>(ch[0] - '0');
  uint8_t digit1 = static_cast<uint8_t>(ch[1] - '0');
  uint8_t digit2 = static_cast<uint8_t>(ch[2] - '0');
  uint8_t digit3 = static_cast<uint8_t>(ch[3] - '0');
  if (digit0 < 10 && digit1 < 10 && digit2 < 10 && digit3 < 10) {
    *out = static_cast<int32_t>(digit0) * 1000 +
           static_cast<int32_t>(digit1) * 100 +
           static_cast<int32_t>(digit2) * 10 +
           static_cast<int32_t>(digit3);
    return true;
  }
  return false;
}

static bool parse_2digits(const char* ch, int32_t* out) {
  uint8_t digit0 = static_cast<uint8_t>(ch[0] - '0');
  uint8_t digit1 = static_cast<uint8_t>(ch[1] - '0');
  if (digit0 < 10 && digit1 < 10) {
    *out = static_cast<int32_t>(digit0) * 10 +
           static_cast<int32_t>(digit1);
    return true;
  }
  return false;
}


static void parse_time64_iso(const ParseContext& ctx) {
  const char* ch = ctx.ch;
  const char* eof = ctx.eof;
  int32_t year = 0,
          month = 0,
          day = 0,
          hours = 0,
          minutes = 0,
          seconds = 0,
          nanos = 0;
  bool ok = (ch + 18 < eof) &&
            parse_4digits(ch, &year) && (ch[4] == '-') &&
            parse_2digits(ch + 5, &month) && (ch[7] == '-') &&
            parse_2digits(ch + 8, &day) &&
            (year >= 1677 && year <= 2262) &&
            (month >= 1 && month <= 12) &&
            (day >= 1 && day <= hh::last_day_of_month(year, month)) &&
            (ch[10] == 'T' || ch[10] == ' ') &&
            parse_2digits(ch + 11, &hours) && (ch[13] == ':') &&
            parse_2digits(ch + 14, &minutes) && (ch[16] == ':') &&
            parse_2digits(ch + 17, &seconds) &&
            (hours < 24 && minutes < 60 && seconds < 60) &&
            (ch += 19);
  if (ok) {
    if (ch < eof && *ch == '.') {
      ch++;  // skip '.'
      int ndigits = 0;
      while (ch < eof) {
        uint8_t digit = static_cast<uint8_t>(*ch - '0');
        if (digit >= 10) break;
        if (ndigits < 9) {  // accumulate no more than 9 digits
          nanos = nanos*10 + static_cast<int32_t>(digit);
          ndigits++;
        }
        ch++;
      }
      while (ndigits < 9) {
        nanos *= 10;
        ndigits++;
      }
    }
    // handle AM/PM suffixes. These are not in ISO, but they occur often
    // in the future we may handle them in a separate parser.
    bool spaceTaken = (ch + 3 <= eof) && (*ch == ' ') && ch++;
    if (ch + 2 <= eof) {
      if ((ch[0] == 'A' && ch[1] == 'M') || (ch[0] == 'a' && ch[1] == 'm')) {
        ok = (hours > 0 && hours <= 12);
        ch += 2;
        if (hours == 12) hours = 0;
      }
      else if ((ch[0] == 'P' && ch[1] == 'M') || (ch[0] == 'p' && ch[1] == 'm')) {
        ok = (hours > 0 && hours <= 12);
        ch += 2;
        if (hours < 12) hours += 12;
      }
      else if (spaceTaken) {
        ch--;
      }
    }
  }
  if (ok) {
    int32_t days = hh::days_from_civil(year, month, day);
    ctx.target->int64 =
        NANOSECONDS_PER_DAY * days +
        NANOSECONDS_PER_SECOND * ((hours*60 + minutes)*60 + seconds) +
        nanos;
    ctx.ch = ch;
  } else {
    ctx.target->int64 = NA_INT64;
  }
}

REGISTER_PARSER(PT::Time64ISO)
    ->parser(parse_time64_iso)
    ->name("Time64/ISO")
    ->code('T')
    ->type(Type::time64())
    ->successors({PT::Str32});



bool parse_time64_iso(const char* ch, const char* end, int64_t* out) {
  int32_t year = 0,
          month = 0,
          day = 0,
          hours = 0,
          minutes = 0,
          seconds = 0,
          nanos = 0;
  bool ok = (ch + 18 < end) &&
            parse_4digits(ch, &year) && (ch[4] == '-') &&
            parse_2digits(ch + 5, &month) && (ch[7] == '-') &&
            parse_2digits(ch + 8, &day) &&
            (year >= 1677 && year <= 2262) &&
            (month >= 1 && month <= 12) &&
            (day >= 1 && day <= hh::last_day_of_month(year, month)) &&
            (ch[10] == 'T' || ch[10] == ' ') &&
            parse_2digits(ch + 11, &hours) && (ch[13] == ':') &&
            parse_2digits(ch + 14, &minutes) && (ch[16] == ':') &&
            parse_2digits(ch + 17, &seconds) &&
            (hours < 24 && minutes < 60 && seconds < 60) &&
            (ch += 19);
  if (ok) {
    if (ch < end && *ch == '.') {
      ch++;  // skip '.'
      int ndigits = 0;
      while (ch < end) {
        uint8_t digit = static_cast<uint8_t>(*ch - '0');
        if (digit >= 10) break;
        if (ndigits < 9) {  // accumulate no more than 9 digits
          nanos = nanos*10 + static_cast<int32_t>(digit);
          ndigits++;
        }
        ch++;
      }
      while (ndigits < 9) {
        nanos *= 10;
        ndigits++;
      }
    }
    // handle AM/PM suffixes. These are not in ISO, but they occur often
    // in the future we may handle them in a separate parser.
    bool spaceTaken = (ch + 3 <= end) && (*ch == ' ') && ch++;
    if (ch + 2 <= end) {
      if ((ch[0] == 'A' && ch[1] == 'M') || (ch[0] == 'a' && ch[1] == 'm')) {
        ok = (hours > 0 && hours <= 12);
        ch += 2;
        if (hours == 12) hours = 0;
      }
      else if ((ch[0] == 'P' && ch[1] == 'M') || (ch[0] == 'p' && ch[1] == 'm')) {
        ok = (hours > 0 && hours <= 12);
        ch += 2;
        if (hours < 12) hours += 12;
      }
      else if (spaceTaken) {
        ch--;
      }
    }
  }
  if (ok) {
    *out = NANOSECONDS_PER_DAY * hh::days_from_civil(year, month, day) +
           NANOSECONDS_PER_SECOND * ((hours*60 + minutes)*60 + seconds) +
           nanos;
  }
  return ok;
}


//------------------------------------------------------------------------------
// Tests
//------------------------------------------------------------------------------
#ifdef DTTEST
  static constexpr int64_t NA = NA_INT64;
  static constexpr int64_t MILLI = 1000000LL;
  static constexpr int64_t SECS = 1000000000LL;
  static constexpr int64_t HOURS = 3600LL * SECS;
  static constexpr int64_t DAYS = 24LL * HOURS;

  static void check(const char* input, int64_t expected_value, size_t expected_advance) {
    ParseContext ctx;
    field64 output;
    ctx.ch = input;
    ctx.eof = input + std::strlen(input);
    ctx.target = &output;
    parse_time64_iso(ctx);
    auto actual_value = output.int64;
    auto actual_advance = static_cast<size_t>(ctx.ch - input);
    ASSERT_EQ(actual_value, expected_value) << " when parsing '" << input << "'";
    ASSERT_EQ(actual_advance, expected_advance) << " when parsing '" << input << "'";
  }

  TEST(fread, test_time64_iso_basic) {
    check("", NA, 0);
    check("1970-01-01 00:00:00", 0, 19);
    check("1970-01-01T00:00:00", 0, 19);
    check("1970-01-01T00:00:00:", 0, 19);
    check("1970-01-01T00:00:001", 0, 19);
    check("1970-01-01T00:00:00.3", 300*MILLI, 21);
    check("1970-01-01T00:00:00.3 ", 300*MILLI, 21);
    check("1970-01-01T00:00:00.3200", 320*MILLI, 24);
    check("1970-01-01T00:00:00.123456789", 123456789LL, 29);
    check("1970-01-01T00:00:00.123456789333", 123456789LL, 32);
    check("2021-03-31 12:59:59", 18717*DAYS + 46799*SECS, 19);
  }

  TEST(fread, test_time64_iso_ampm) {
    check("1970-01-01 12:00:00 AM", 0, 22);
    check("1970-01-01 12:00:00 am", 0, 22);
    check("1970-01-01 12:00:00AM", 0, 21);
    check("1970-01-01 12:00:00am", 0, 21);
    check("1970-01-01 12:00:00 AM", 0*HOURS, 22);
    check("1970-01-01 01:00:00 AM", 1*HOURS, 22);
    check("1970-01-01 02:00:00 AM", 2*HOURS, 22);
    check("1970-01-01 03:00:00 AM", 3*HOURS, 22);
    check("1970-01-01 04:00:00 AM", 4*HOURS, 22);
    check("1970-01-01 05:00:00 AM", 5*HOURS, 22);
    check("1970-01-01 06:00:00 AM", 6*HOURS, 22);
    check("1970-01-01 07:00:00 AM", 7*HOURS, 22);
    check("1970-01-01 08:00:00 AM", 8*HOURS, 22);
    check("1970-01-01 09:00:00 AM", 9*HOURS, 22);
    check("1970-01-01 10:00:00 AM", 10*HOURS, 22);
    check("1970-01-01 11:00:00 AM", 11*HOURS, 22);
    check("1970-01-01 12:00:00 PM", 12*HOURS, 22);
    check("1970-01-01 01:00:00 PM", 13*HOURS, 22);
    check("1970-01-01 02:00:00 PM", 14*HOURS, 22);
    check("1970-01-01 03:00:00 PM", 15*HOURS, 22);
    check("1970-01-01 04:00:00 PM", 16*HOURS, 22);
    check("1970-01-01 05:00:00 PM", 17*HOURS, 22);
    check("1970-01-01 06:00:00 PM", 18*HOURS, 22);
    check("1970-01-01 07:00:00 PM", 19*HOURS, 22);
    check("1970-01-01 08:00:00 PM", 20*HOURS, 22);
    check("1970-01-01 09:00:00 PM", 21*HOURS, 22);
    check("1970-01-01 10:00:00 PM", 22*HOURS, 22);
    check("1970-01-01 11:00:00 PM", 23*HOURS, 22);
  }

  TEST(fread, test_time64_iso_invalid) {
    check("1990-00-01 00:00:00", NA, 0);
    check("1990-13-01 00:00:00", NA, 0);
    check("1990-01-00 00:00:00", NA, 0);
    check("1990-01-33 00:00:00", NA, 0);
    check("9999-01-01 00:00:00", NA, 0);
    check("1980-01-01 00:00:00 AM", NA, 0);
    check("1980-01-01 13:00:00 AM", NA, 0);
    check("1980-01-01 23:00:00 AM", NA, 0);
    check("1980-01-01 00:00:00 PM", NA, 0);
    check("1980-01-01 13:00:00 PM", NA, 0);
    check("1980-01-01 23:00:00 PM", NA, 0);
    check("1990-01-01 24:00:00", NA, 0);
    check("1990-01-01 23:60:00", NA, 0);
    check("1990-01-01 23:59:60", NA, 0);
  }
#endif



}}  // namespace dt::read::
