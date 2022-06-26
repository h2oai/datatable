//------------------------------------------------------------------------------
// Copyright 2018-2022 H2O.ai
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
#ifndef dt_UTILS_ASSERT_h
#define dt_UTILS_ASSERT_h
#include <cstdio>
#include "utils/macros.h"
#ifdef NO_DT
  #include <stdexcept>
  #include <iostream>
#else
  #include "utils/exceptions.h"
#endif

#if DT_OS_MACOS
  #include <unistd.h>
#endif


// First, fix the NDEBUG macro.
// This macro, if present, disables all assert statements. Unfortunately, python
// may add it as a default compilation flag, so in order to combat this we have
// defined a new `DT_DEBUG` macro. This macro may be passed from the Makefile,
// and if declared it means to ignore the NDEBUG macro even if it is present.
#undef NDEBUG
#ifdef DT_DEBUG
  #undef DT_DEBUG
  #define DT_DEBUG 1
#else
  #define DT_DEBUG 0
  #define NDEBUG 1
#endif

// This will include the standard `assert` and `static_assert` macros.
#include <assert.h>


// Here we also define the `xassert` macro, which behaves similarly to `assert`,
// however it throws exceptions instead of terminating the program
#if DT_DEBUG
  #ifdef NO_DT
    #define wassert(EXPRESSION) \
      if (!(EXPRESSION)) { \
        std::cerr << "Assertion '" #EXPRESSION "' failed in " __FILE__ ", line " \
                  << __LINE__ << "\n"; \
      } ((void)(0))
  #else
    #define wassert(EXPRESSION) \
      if (!(EXPRESSION)) { \
        (AssertionError() << "Assertion '" #EXPRESSION "' failed in " \
          << __FILE__ << ", line " << __LINE__).to_stderr(); \
      } ((void)(0))
  #endif

  #ifdef NO_DT
    #define xassert(EXPRESSION) \
      if (!(EXPRESSION)) { \
        throw std::runtime_error("Assertion '" #EXPRESSION "' failed in " __FILE__ \
                                 ", line " + std::to_string(__LINE__)); \
      } ((void)(0))
  #else
    #define xassert(EXPRESSION) \
      if (!(EXPRESSION)) { \
        throw AssertionError() << "Assertion '" #EXPRESSION "' failed in " \
            << __FILE__ << ", line " << __LINE__; \
      } ((void)(0))
  #endif

#else
  #define wassert(EXPRESSION) ((void)0)
  #define xassert(EXPRESSION) ((void)0)
#endif


// XAssert() macro is similar to xassert(), except it works both in
// debug and in production.
#ifdef NO_DT
  #define XAssert(EXPRESSION) \
    if (!(EXPRESSION)) { \
      throw std::runtime_error("Assertion '" #EXPRESSION "' failed in " __FILE__ \
                               ", line " + std::to_string(__LINE__)); \
    } ((void)(0))
#else
  #define XAssert(EXPRESSION) \
    if (!(EXPRESSION)) { \
      throw AssertionError() << "Assertion '" #EXPRESSION "' failed in " \
          << __FILE__ << ", line " << __LINE__; \
    } ((void)(0))
#endif


#endif
