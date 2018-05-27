//------------------------------------------------------------------------------
// This code was taken from
//   https://github.com/miloyip/itoa-benchmark/blob/master/src/branchlut2.cpp
// (MIT licensed)
//
// Copyright (C) 2014 Milo Yip
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//------------------------------------------------------------------------------
#ifndef dt_CSV_ITOA_h
#define dt_CSV_ITOA_h


const char gDigitsLut[200] = {
  '0','0','0','1','0','2','0','3','0','4','0','5','0','6','0','7','0','8','0','9',
  '1','0','1','1','1','2','1','3','1','4','1','5','1','6','1','7','1','8','1','9',
  '2','0','2','1','2','2','2','3','2','4','2','5','2','6','2','7','2','8','2','9',
  '3','0','3','1','3','2','3','3','3','4','3','5','3','6','3','7','3','8','3','9',
  '4','0','4','1','4','2','4','3','4','4','4','5','4','6','4','7','4','8','4','9',
  '5','0','5','1','5','2','5','3','5','4','5','5','5','6','5','7','5','8','5','9',
  '6','0','6','1','6','2','6','3','6','4','6','5','6','6','6','7','6','8','6','9',
  '7','0','7','1','7','2','7','3','7','4','7','5','7','6','7','7','7','8','7','9',
  '8','0','8','1','8','2','8','3','8','4','8','5','8','6','8','7','8','8','8','9',
  '9','0','9','1','9','2','9','3','9','4','9','5','9','6','9','7','9','8','9','9'
};


#define MIDDLE2(n) \
  do { \
    int t2 = static_cast<int>(n) * 2; \
    *ch++ = gDigitsLut[t2]; \
    *ch++ = gDigitsLut[t2 + 1]; \
  } while(0)

#define BEGIN2(n) \
  do { \
    int t2 = static_cast<int>(n); \
    if (t2 < 10) { \
      *ch++ = '0' + static_cast<char>(t2); \
    } else { \
      t2 *= 2; \
      *ch++ = gDigitsLut[t2]; \
      *ch++ = gDigitsLut[t2 + 1]; \
    } \
  } while(0)

#define BEGIN4(n) \
  do { \
    int t4 = static_cast<int>(n); \
    if (t4 < 100) { \
      BEGIN2(t4); \
    } else { \
      BEGIN2(t4 / 100); \
      MIDDLE2(t4 % 100); \
    } \
  } while(0)

#define MIDDLE4(n) \
  do { \
    int t4 = static_cast<int>(n); \
    MIDDLE2(t4 / 100); \
    MIDDLE2(t4 % 100); \
  } while(0)

#define BEGIN8(n) \
  do { \
    uint32_t t8 = static_cast<uint32_t>(n); \
    if (t8 < 10000) { \
      BEGIN4(t8); \
    } else { \
      BEGIN4(t8 / 10000); \
      MIDDLE4(t8 % 10000); \
    } \
  } while(0)

#define MIDDLE8(n) \
  do { \
    uint32_t t8 = static_cast<uint32_t>(n); \
    MIDDLE4(t8 / 10000); \
    MIDDLE4(t8 % 10000); \
  } while(0)

#define MIDDLE16(n) \
  do { \
    uint64_t t16 = static_cast<uint64_t>(n); \
    MIDDLE8(t16 / 100000000); \
    MIDDLE8(t16 % 100000000); \
  } while(0)



inline void itoa(char **pch, int32_t value) {
  char *ch = *pch;
  if (value < 0) {
    *ch++ = '-';
    value = -value;
  }

  uint32_t uvalue = static_cast<uint32_t>(value);
  if (uvalue < 100000000) {
    BEGIN8(uvalue);
  } else {
    BEGIN2(uvalue / 100000000);
    MIDDLE8(uvalue % 100000000);
  }
  *pch = ch;
}


inline void ltoa(char **pch, int64_t value) {
  char *ch = *pch;
  if (value < 0) {
    *ch++ = '-';
    value = -value;
  }

  uint64_t uvalue = static_cast<uint64_t>(value);
  if (uvalue < 100000000) {
    BEGIN8(uvalue);
  } else if (uvalue < 10000000000000000) {
    BEGIN8(uvalue / 100000000);
    MIDDLE8(uvalue % 100000000);
  } else {
    BEGIN4(uvalue / 10000000000000000);
    MIDDLE16(uvalue % 10000000000000000);
  }
  *pch = ch;
}


#endif
