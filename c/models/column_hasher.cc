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
#include <float.h>


/*
* Abstract Hasher class constructor and destructor.
*/
Hasher::Hasher(const Column* col) : ri(col->rowindex()) {}
Hasher::~Hasher() {}


/*
* Hash booleans by casting them to `uint64_t`.
*/
HasherBool::HasherBool(const Column* col) : Hasher(col) {
  values = dynamic_cast<const BoolColumn*>(col)->elements_r();
}


uint64_t HasherBool::hash(size_t row) const {
  size_t i = ri[row];
  int8_t value = (i == RowIndex::NA)? GETNA<int8_t>() : values[i];
  uint64_t h = static_cast<uint64_t>(value);
  return h;
}


/*
* Hash integers by casting them to `uint64_t`.
*/
template <typename T>
HasherInt<T>::HasherInt(const Column* col) : Hasher(col) {
  values = static_cast<const T*>(col->data());
}


template <typename T>
uint64_t HasherInt<T>::hash(size_t row) const {
  size_t i = ri[row];
  T value = (i == RowIndex::NA)? GETNA<T>() : values[i];
  uint64_t h = static_cast<uint64_t>(value);
  return h;
}


/*
* Hash floats by reinterpreting their bit representation as `uint64_t`.
* TODO: also support some binning here.
*/
template <typename T>
HasherFloat<T>::HasherFloat(const Column* col, unsigned char mantissa_nbits) :
Hasher(col), shift_nbits(DBL_MANT_DIG - mantissa_nbits) {
  values = static_cast<const T*>(col->data());
}


template <typename T>
uint64_t HasherFloat<T>::hash(size_t row) const {
  size_t i = ri[row];
  T value = (i == RowIndex::NA)? GETNA<T>() : values[i];
  uint64_t h;
  double x = static_cast<double>(value);
  std::memcpy(&h, &x, sizeof(double));
  return h >> shift_nbits;
}


/*
* Hash strings using Murmur hash function.
*/
template <typename T>
HasherString<T>::HasherString(const Column* col) : Hasher(col){
  auto scol = dynamic_cast<const StringColumn<T>*>(col);
  strdata = scol->strdata();
  offsets = scol->offsets();
}


template <typename T>
uint64_t HasherString<T>::hash(size_t row) const {
  size_t i = ri[row];
  if (i == RowIndex::NA) {
    return static_cast<uint64_t>(GETNA<T>());
  } else {
    if (ISNA<T>(offsets[i])) {
      return static_cast<uint64_t>(GETNA<T>());
    }
    const T strstart = offsets[i - 1] & ~GETNA<T>();
    const char* c_str = strdata + strstart;
    T len = offsets[i] - strstart;
    return hash_murmur2(c_str, len * sizeof(char), 0);
  }
}

template class HasherInt<int8_t>;
template class HasherInt<int16_t>;
template class HasherInt<int32_t>;
template class HasherInt<int64_t>;
template class HasherFloat<float>;
template class HasherFloat<double>;
template class HasherString<uint32_t>;
template class HasherString<uint64_t>;
