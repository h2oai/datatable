//------------------------------------------------------------------------------
// Copyright 2020 H2O.ai
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
#ifndef dt_READ_COLINFO_h
#define dt_READ_COLINFO_h
#include "_dt.h"
namespace dt {
namespace read {


struct StrInfo {
  size_t size, write_at;
};

struct BoolInfo {
  size_t count0, count1;
};

struct IntInfo {
  int64_t min, max;
};

struct FloatInfo {
  double min, max;
};


/**
  * Helper struct that is used in OutputColumn and ThreadContext.
  * It holds per-column stats information.
  */
struct ColInfo {
  size_t na_count;
  union {
    BoolInfo  b;
    IntInfo   i;
    FloatInfo f;
    StrInfo   str;
  };

  ColInfo() : na_count(0) {}
  ColInfo(const ColInfo&) = default;
};




}}  // namespace dt::read
#endif
