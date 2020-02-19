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
#ifndef dt_SORT_COMMON_h
#define dt_SORT_COMMON_h
#include <cstddef>           // std::size_t
#include "utils/assert.h"    // xassert
#include "buffer.h"          // Buffer
namespace dt {
namespace sort {

using std::size_t;

static constexpr size_t MAX_NROWS_INT32 = 0x7FFFFFFF;
static constexpr size_t INSERTSORT_NROWS = 16;


/**
  * Simple struct-like class which represents a raw pointer viewed
  * as an array of elements of type `T`. The pointer is not owned.
  *
  * The main difference between `array<T>` and a simple pointer `T*`
  * is that the former also knows its size.
  */
template <typename T>
class array {
  public:
    T* ptr;
    size_t size;

  public:
    array() : ptr(nullptr), size(0) {}
    array(T* p, size_t n) : ptr(p), size(n) {}
    array(const array&) = default;
    array& operator=(const array&) = default;

    array(const Buffer& buf)
      : ptr(static_cast<T*>(buf.xptr())),
        size(buf.size() / sizeof(T))
    { xassert(buf.size() % sizeof(T) == 0); }

    array<T> subset(size_t start, size_t length) {
      xassert(start + length <= size);
      return array<T>(ptr + start, length);
    }
};



}}  // namespace dt::sort
#endif
