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


static constexpr size_t MAX_NROWS_INT32 = 0x7FFFFFFF;
static constexpr size_t INSERTSORT_NROWS = 16;


enum class Mode {
  SINGLE_THREADED,
  PARALLEL
};

enum class Direction {
  ASCENDING,
  DESCENDING
};


/**
  * Simple struct-like class which represents a raw pointer viewed
  * as an array of elements of type `T`. The pointer is not owned.
  *
  * The main difference between `array<T>` and a simple pointer `T*`
  * is that the array also knows its size.
  */
template <typename T>
class array
{
  private:
    T* ptr_;
    size_t size_;

  public:
    array() : ptr_(nullptr), size_(0) {}
    array(const array&) = default;
    array& operator=(const array&) = default;

    array(T* p, size_t n)
      : ptr_(p), size_(n)
    {
      xassert((ptr_ == nullptr) == (size_ == 0));
    }

    array(const Buffer& buf, size_t offset = 0)
      : ptr_(static_cast<T*>(buf.xptr()) + offset),
        size_(buf.size() / sizeof(T) - offset)
    {
      xassert(buf.size() % sizeof(T) == 0);
      xassert(buf.size() >= offset * sizeof(T));
    }

    array<T> subset(size_t start, size_t length) {
      xassert(start + length <= size_);
      return array<T>(ptr_ + start, length);
    }


  //------------------------------------
  // Properties
  //------------------------------------
  public:
    size_t size() const noexcept {
      return size_;
    }

    T* start() const noexcept {
      return ptr_;
    }

    T* end() const noexcept {
      return ptr_ + size_;
    }

    operator bool() const noexcept {
      return (ptr_ != nullptr);
    }

    T& operator[](size_t i) const {
      xassert(i < size_);
      return ptr_[i];
    }

    T& operator[](int32_t i) const {
      xassert(static_cast<size_t>(i) < size_);
      return ptr_[i];
    }

    T& operator[](int64_t i) const {
      xassert(static_cast<size_t>(i) < size_);
      return ptr_[i];
    }

    bool intersects(const array<T>& other) const {
      return (other.start() < end()) && (start() < other.end());
    }
};



}}  // namespace dt::sort
#endif
