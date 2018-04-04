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

#ifdef NDEBUG
  // Macro NDEBUG, if present, disables all assert statements. Unfortunately,
  // Python turns on this macro by default, so we need to turn it off
  // explicitly.
  #ifdef NONDEBUG
    #undef NDEBUG
  #endif
#endif

#include <assert.h>  // assert, dt_static_assert

#ifdef NDEBUG
  #define xassert(EXPRESSION)
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
