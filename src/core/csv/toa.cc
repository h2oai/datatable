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
#include "csv/toa.h"
#include "lib/hh/date.h"
#include "utils/assert.h"



void int8_toa(char** pch, int8_t value) {
  char* ch = *pch;
  if (value < 0) {
    *ch++ = '-';
    value = -value;
  }
  if (value >= 100) {  // the range of `value` is up to 128
    *ch++ = '1';
    int d = value/10;
    *ch++ = static_cast<char>(d) - 10 + '0';
    value -= static_cast<int8_t>(d*10);
  } else if (value >= 10) {
    int d = value/10;
    *ch++ = static_cast<char>(d) + '0';
    value -= static_cast<int8_t>(d*10);
  }
  *ch++ = static_cast<char>(value) + '0';
  *pch = ch;
}


void int16_toa(char** pch, int16_t value) {
  if (value == 0) {
    *((*pch)++) = '0';
    return;
  }
  char* ch = *pch;
  if (value < 0) {
    *ch++ = '-';
    value = -value;
  }
  unsigned int r = (value < 1000)? 2 : 4;
  for (; value < DIVS32[r]; r--);
  for (; r; r--) {
    int d = value / DIVS32[r];
    *ch++ = static_cast<char>(d) + '0';
    value -= static_cast<int16_t>(d * DIVS32[r]);
  }
  *ch = static_cast<char>(value) + '0';
  *pch = ch + 1;
}


void date32_toa(char** pch, int32_t value) {
  auto ymd = hh::civil_from_days(value);
  char* ch = *pch;
  if (ymd.year < 0) {
    *ch++ = '-';
    ymd.year = -ymd.year;
  }
  else if (ymd.year < 1000) {
    *ch++ = '0';
  }
  if (ymd.year < 100) {
    *ch++ = '0';
    if (ymd.year < 10) {
      *ch++ = '0';
    }
  }
  itoa(&ch, ymd.year);
  *ch++ = '-';
  if (ymd.month < 10) {
    *ch++ = '0';
    *ch++ = static_cast<char>('0' + ymd.month);
  } else {
    *ch++ = static_cast<char>('0' + (ymd.month / 10));
    *ch++ = static_cast<char>('0' + (ymd.month % 10));
  }
  *ch++ = '-';
  if (ymd.day < 10) {
    *ch++ = '0';
    *ch++ = static_cast<char>('0' + ymd.day);
  } else {
    *ch++ = static_cast<char>('0' + (ymd.day / 10));
    *ch++ = static_cast<char>('0' + (ymd.day % 10));
  }
  *pch = ch;
}


// Maximum space needed: 29 chars
//   <date> : 10 chars
//   T      : 1
//   <time> : 8 chars
//   .      : 1
//   <ns>   : 9
//
void time64_toa(char** pch, int64_t time) {
  static constexpr int64_t NANOSECONDS_PER_SECOND = 1000000000LL;
  static constexpr int64_t NANOSECONDS_PER_DAY = 24LL * 3600LL * 1000000000LL;

  auto days = (time >= 0)? time / NANOSECONDS_PER_DAY
                         : (time + 1) / NANOSECONDS_PER_DAY - 1;
  auto time_of_day = time - days * NANOSECONDS_PER_DAY;
  xassert(time_of_day >= 0);
  auto ns = time_of_day % NANOSECONDS_PER_SECOND;
  time_of_day /= NANOSECONDS_PER_SECOND;
  auto seconds = time_of_day % 60;
  time_of_day /= 60;
  auto minutes = time_of_day % 60;
  time_of_day /= 60;
  auto hours = time_of_day;

  xassert(days < 110000 && days > -110000);
  date32_toa(pch, static_cast<int>(days));
  char* ch = *pch;
  *ch++ = 'T';
  *ch++ = static_cast<char>('0' + (hours / 10));
  *ch++ = static_cast<char>('0' + (hours % 10));
  *ch++ = ':';
  *ch++ = static_cast<char>('0' + (minutes / 10));
  *ch++ = static_cast<char>('0' + (minutes % 10));
  *ch++ = ':';
  *ch++ = static_cast<char>('0' + (seconds / 10));
  *ch++ = static_cast<char>('0' + (seconds % 10));
  if (ns) {
    *ch++ = '.';
    int64_t factor = NANOSECONDS_PER_SECOND / 10;
    while (ns) {
      auto digit = ns / factor;
      xassert(digit < 10);
      *ch++ = static_cast<char>('0' + digit);
      ns -= digit * factor;
      factor /= 10;
    }
  }
  *pch = ch;
}
