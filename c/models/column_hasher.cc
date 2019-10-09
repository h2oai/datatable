//------------------------------------------------------------------------------
// Copyright 2018 H2O.ai
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
#include "models/column_hasher.h"

/**
 *  Abstract Hasher class constructor and destructor.
 */
Hasher::Hasher(const Column& col) : column(col) {}
Hasher::~Hasher() {}


/**
 *  Hash integers by casting them to `uint64_t`.
 */
uint64_t HasherInt::hash(size_t row) const {
  int64_t value;
  bool isvalid = column.get_element(row, &value);
  return isvalid? static_cast<uint64_t>(value)
                : GETNA<uint64_t>();
}


/**
 *  Hash floats by interpreting their bit representation as `uint64_t`,
 *  also do mantissa binning here.
 */
HasherFloat::HasherFloat(const Column& col, int shift_nbits_in)
  : Hasher(col), shift_nbits(shift_nbits_in) {}


uint64_t HasherFloat::hash(size_t row) const {
  double value;  // float or double
  uint64_t h = GETNA<uint64_t>();
  bool isvalid = column.get_element(row, &value);
  if (isvalid) {
    double x = static_cast<double>(value);
    std::memcpy(&h, &x, sizeof(double));
    h >>= shift_nbits;
  }
  return h;
}


/**
 *  Hash strings using Murmur hash function.
 */
uint64_t HasherString::hash(size_t row) const {
  CString value;
  bool isvalid = column.get_element(row, &value);
  return isvalid? hash_murmur2(value.ch, static_cast<size_t>(value.size))
                : GETNA<uint64_t>();
}
