//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#ifndef dt_UTILS_ARRAY_h
#define dt_UTILS_ARRAY_h
#include <algorithm>      // std::swap
#include "buffer.h"
#include "utils/alloc.h"  // dt::realloc
#include "utils/exceptions.h"

namespace dt
{

/**
 * Simple wrapper around C primitive array `T*`. It encapsulates functions
 * `malloc`, `realloc` and `free` making sure that the array is deallocated
 * when necessary, and that it throws an exception if memory allocation fails.
 *
 * Unlike `std::vector<T>`, this class does not distinguish between size and
 * capacity, and also it doesn't attempt to initialize elements to 0 when
 * array is created. Unlike `std::array<T, N>`, dt::array has dynamic size.
 *
 * Note that this class is designed to be used with primitive types only, in
 * particular it cannot hold objects (or pointers to objects) since it doesn't
 * invoke constructor/destructor on its elements.
 *
 * Examples
 * --------
 * array<int> a1;           // array of size 0
 * array<int> a2(10);       // array of size 10, similar to new int[10];
 * a1.resize(10);           // now a1 also has 10 elements
 * a1.size() == a2.size();  // both return 10
 * a1[0] = 1;               // assignment to individual "cells"
 * a1[9] = 1000;            // no bound checks are performed!
 * int* ptr = a1.data();    // convert to plain C pointer
 * // free(ptr);            // error: the pointer still belongs to array!
 * array<int64_t> a3 = a1.cast<int64_t>(); // convert to a different array
 *                          // array will keep its size but not values.
 * std::cout << a3.size();  // 5
 * std::cout << a1.size();  // 0 (a1 no longer owns the data)
 */
template <typename T> class array
{
  private:
    T* x;
    size_t n;
    bool owned;
    int64_t : 56;

  public:
    array(size_t len = 0) : x(nullptr), n(0), owned(true) { resize(len); }
    array(size_t len, const T* ptr)
      : x(const_cast<T*>(ptr)), n(len), owned(false) {}
    array(size_t len, const T* ptr, bool own)
      : x(const_cast<T*>(ptr)), n(len), owned(own) {}
    ~array() { if (owned) dt::free(x); }
    // copy-constructor and assignment are forbidden
    array(const array<T>&) = delete;
    array<T>& operator=(const array<T>&) = delete;
    // move-constructor and move-assignment are ok
    array(array<T>&& other) : array() { swap(*this, other); }
    array<T>& operator=(array<T>&& other) { swap(*this, other); return *this; }

    // see https://stackoverflow.com/questions/5695548
    friend void swap(array<T>& first, array<T>& second) noexcept {
      using std::swap;
      swap(first.x, second.x);
      swap(first.n, second.n);
      swap(first.owned, second.owned);
    }

    template <typename S> array<S> cast() {
      array<S> res;
      res.n = n * sizeof(T) / sizeof(S);
      res.x = reinterpret_cast<S*>(x);
      res.owned = owned;
      x = nullptr;
      return res;
    }

    Buffer to_memoryrange() {
      void* ptr = x;
      size_t size = sizeof(T) * n;
      x = nullptr;
      n = 0;
      if (owned) return Buffer::acquire(ptr, size);
      else       return Buffer::external(ptr, size);
    }

    // Standard operators
    T& operator[](size_t i) {
      xassert(i < n);
      return x[i];
    }

    const T& operator[](size_t i) const {
      xassert(i < n);
      return x[i];
    }

    operator bool() const { return x; }

    T* data() { return x; }
    const T* data() const { return x; }
    bool data_owned() const { return owned; }

    size_t size() const { return n; }

    void resize(size_t newn) {
      if (newn == n) return;
      if (!owned) {
        throw MemoryError() << "Cannot resize array: not owned";
      }
      x = dt::arealloc<T>(x, newn);
      n = newn;
    }

    void* release() {
      void* ptr = x;
      x = nullptr;
      return ptr;
    }

    void ensuresize(size_t newn) {
      if (newn > n) resize(newn);
    }
};


};  // namespace dt

typedef dt::array<int32_t> arr32_t;
typedef dt::array<int64_t> arr64_t;

#endif
