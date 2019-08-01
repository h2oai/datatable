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
#include "utils/assert.h"


/**
 *  Abstract Hasher class constructor and destructor.
 */
Hasher::Hasher(const OColumn& col) : column(col) {}
Hasher::~Hasher() {}


/**
 *  Hash integers by casting them to `uint64_t`.
 */
template <typename T>
uint64_t HasherInt<T>::hash(size_t row) const {
  T value;
  bool r = column.get_element(row, &value);
  if (r) value = GETNA<T>();
  return static_cast<uint64_t>(value);
}


/**
 *  Hash floats by interpreting their bit representation as `uint64_t`,
 *  also do mantissa binning here.
 */
template <typename T>
HasherFloat<T>::HasherFloat(const OColumn& col, int shift_nbits_in)
  : Hasher(col), shift_nbits(shift_nbits_in) {}


template <typename T>
uint64_t HasherFloat<T>::hash(size_t row) const {
  T value;  // float or double
  uint64_t h;
  bool r = column.get_element(row, &value);
  if (r) value = GETNA<T>();
  double x = static_cast<double>(value);
  std::memcpy(&h, &x, sizeof(double));
  return h >> shift_nbits;
}


/**
 *  Hash strings using Murmur hash function.
 */
uint64_t HasherString::hash(size_t row) const {
  CString value;
  bool r = column.get_element(row, &value);
  return r? GETNA<uint64_t>()
          : hash_murmur2(value.ch, static_cast<size_t>(value.size));
}



template class HasherInt<int32_t>;
template class HasherInt<int64_t>;
template class HasherFloat<float>;
template class HasherFloat<double>;
