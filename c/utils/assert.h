//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#ifndef dt_UTILS_ASSERT_h
#define dt_UTILS_ASSERT_h
#include "utils/exceptions.h"

// First, fix the NDEBUG macro.
// This macro, if present, disables all assert statements. Unfortunately, python
// may add it as a default compilation flag, so in order to combat this we have
// defined a new `DTDEBUG` macro. This macro may be passed from the Makefile,
// and if declared it means to ignore the NDEBUG macro even if it is present.
#ifdef NDEBUG
  #ifdef DTDEBUG
    #undef NDEBUG
  #endif
#endif

// This will include the standard `assert` and `static_assert` macros.
#include <assert.h>


// Here we also define the `xassert` macro, which behaves similarly to `assert`,
// however it throws exceptions instead of terminating the program
#ifdef NDEBUG
  #define xassert(EXPRESSION) ((void)0)
#else
  #define xassert(EXPRESSION) \
    if (!(EXPRESSION)) { \
      throw AssertionError() << "Assertion '" << #EXPRESSION << "' failed in " \
          << __FILE__ << ", line " << __LINE__; \
    }
#endif


#ifdef static_assert
  #define dt_static_assert static_assert
#else
  // Some system libraries fail to define this macro :( At least 3 people have
  // reported having this problem on the first day of testing, so it's not
  // that uncommon.
  // In this case we just disable the checks.
  #define dt_static_assert(x, y)
#endif


#endif
