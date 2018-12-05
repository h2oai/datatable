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
#ifndef dt_EXTRAS_HASH_h
#define dt_EXTRAS_HASH_h
#include "py_datatable.h"
#include "extras/murmurhash.h"


/*
* An abstract class for all the hashers.
*/
class Hash {
  public:
    virtual ~Hash();
    virtual uint64_t hash(size_t row) const = 0;
};


/*
* Class to hash booleans.
*/
class HashBool : public Hash {
  private:
    const int8_t* values;
  public:
    explicit HashBool(const Column*);
    uint64_t hash(size_t row) const override;
};


/*
* Template class to hash integers.
*/
template <typename T>
class HashInt : public Hash {
  private:
    const T* values;
  public:
    explicit HashInt(const Column*);
    uint64_t hash(size_t row) const override;
};


template <typename T>
HashInt<T>::HashInt(const Column* col) {
  values = dynamic_cast<const IntColumn<T>*>(col)->elements_r();
}


template <typename T>
uint64_t HashInt<T>::hash(size_t row) const {
  uint64_t h = static_cast<uint64_t>(values[row]);
  return h;
}


/*
*  Template class to hash floats.
*/
template <typename T>
class HashFloat : public Hash {
  private:
    const T* values;
  public:
    explicit HashFloat(const Column*);
    uint64_t hash(size_t row) const override;
};


template <typename T>
HashFloat<T>::HashFloat(const Column* col) {
  values = dynamic_cast<const RealColumn<T>*>(col)->elements_r();
}


template <typename T>
uint64_t HashFloat<T>::hash(size_t row) const {
  auto x = static_cast<double>(values[row]);
  uint64_t* h = reinterpret_cast<uint64_t*>(&x);
  return *h;
}


/*
*  Template class to hash strings.
*/
template <typename T>
class HashString : public Hash {
  private:
    const char* strdata;
    const T* offsets;
  public:
    explicit HashString(const Column*);
    uint64_t hash(size_t row) const override;
};


template <typename T>
HashString<T>::HashString(const Column* col) {
  auto scol = dynamic_cast<const StringColumn<T>*>(col);
  strdata = scol->strdata();
  offsets = scol->offsets();
}


template <typename T>
uint64_t HashString<T>::hash(size_t row) const {
  const T strstart = offsets[row - 1] & ~GETNA<T>();
  const char* c_str = strdata + strstart;
  T len = offsets[row] - strstart;
  uint64_t h = hash_murmur2(c_str, len * sizeof(char), 0);
  return h;
}

#endif
