//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#ifndef dt_CSV_TOA_h
#define dt_CSV_TOA_h
#include "csv/dtoa.h"   // dtoa, ftoa, DIVS32
#include "csv/itoa.h"   // itoa, ltoa


inline void btoa(char** pch, int8_t value)
{
  char* ch = *pch;
  if (value < 0) {
    *ch++ = '-';
    value = -value;
  }
  if (value >= 100) {  // the range of `value` is up to 128
    *ch++ = '1';
    int d = value/10;
    *ch++ = static_cast<char>(d) - 10 + '0';
    value -= d*10;
  } else if (value >= 10) {
    int d = value/10;
    *ch++ = static_cast<char>(d) + '0';
    value -= d*10;
  }
  *ch++ = static_cast<char>(value) + '0';
  *pch = ch;
}


inline void htoa(char** pch, int16_t value)
{
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
    value -= d * DIVS32[r];
  }
  *ch = static_cast<char>(value) + '0';
  *pch = ch + 1;
}



//---- Generic -----------------------------------------------------------------

template<typename T>
           inline void toa(char**, T)              { }
template<> inline void toa(char** pch, int8_t x)   { btoa(pch, x); }
template<> inline void toa(char** pch, int16_t x)  { htoa(pch, x); }
template<> inline void toa(char** pch, int32_t x)  { itoa(pch, x); }
template<> inline void toa(char** pch, int64_t x)  { ltoa(pch, x); }
template<> inline void toa(char** pch, float x)    { ftoa(pch, x); }
template<> inline void toa(char** pch, double x)   { dtoa(pch, x); }

#endif
