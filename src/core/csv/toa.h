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
#ifndef dt_CSV_TOA_h
#define dt_CSV_TOA_h
#include "csv/dtoa.h"   // dtoa, ftoa, DIVS32
#include "csv/itoa.h"   // itoa, ltoa


void int8_toa(char** pch, int8_t value);
void int16_toa(char** pch, int16_t value);
void date32_toa(char** pch, int32_t value);
void time64_toa(char** pch, int64_t value);


//---- Generic -----------------------------------------------------------------

template<typename T>
           inline void toa(char**, T)              { }
template<> inline void toa(char** pch, int8_t x)   { int8_toa(pch, x); }
template<> inline void toa(char** pch, int16_t x)  { int16_toa(pch, x); }
template<> inline void toa(char** pch, int32_t x)  { itoa(pch, x); }
template<> inline void toa(char** pch, int64_t x)  { ltoa(pch, x); }
template<> inline void toa(char** pch, float x)    { ftoa(pch, x); }
template<> inline void toa(char** pch, double x)   { dtoa(pch, x); }



#endif
