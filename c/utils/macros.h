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
#ifndef dt_UTILS_MACROS_h
#define dt_UTILS_MACROS_h


#if defined(__clang__)
  #define FALLTHROUGH [[clang::fallthrough]]
#elif defined(__GNUC__) && __GNUC__ >= 7
  #define FALLTHROUGH [[gnu::fallthrough]]
#else
  #define FALLTHROUGH
#endif


// Cache line size: equivalent of std::hardware_destructive_interference_size
// from C++17.
#if defined(__i386__) || defined(__x86_64__)
  #define CACHELINE_SIZE 64
#elif defined(__powerpc64__)
  #define CACHELINE_SIZE 128
#else
  // Reasonable default
  #define CACHELINE_SIZE 64
#endif


// Helper template to replace type `T` with a cache-aligned + padded wrapper
// type. Using this structure may help reduce false sharing
template <typename T>
struct alignas(CACHELINE_SIZE) cache_aligned {
  T v;

  char _padding[(CACHELINE_SIZE - (sizeof(T) % CACHELINE_SIZE))
                % CACHELINE_SIZE];

  cache_aligned() = default;
  cache_aligned(const T& v_) : v(v_) {}
  cache_aligned(T&& v_) : v(std::move(v_)) {}
};



#endif
