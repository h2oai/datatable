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
