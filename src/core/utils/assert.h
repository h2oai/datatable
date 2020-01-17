//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#ifndef dt_UTILS_ASSERT_h
#define dt_UTILS_ASSERT_h
#include <cstdio>
#include "python/obj.h"
#include "utils/exceptions.h"
#include "types.h"


// First, fix the NDEBUG macro.
// This macro, if present, disables all assert statements. Unfortunately, python
// may add it as a default compilation flag, so in order to combat this we have
// defined a new `DTDEBUG` macro. This macro may be passed from the Makefile,
// and if declared it means to ignore the NDEBUG macro even if it is present.
#undef NDEBUG
#ifdef DTDEBUG
  #undef DTDEBUG
  #define DTDEBUG 1
#else
  #define DTDEBUG 0
  #define NDEBUG 1
#endif

// This will include the standard `assert` and `static_assert` macros.
#include <assert.h>


// Here we also define the `xassert` macro, which behaves similarly to `assert`,
// however it throws exceptions instead of terminating the program
#if DTDEBUG
  #define wassert(EXPRESSION) \
    if (!(EXPRESSION)) { \
      (AssertionError() << "Assertion '" #EXPRESSION "' failed in " \
          << __FILE__ << ", line " << __LINE__).to_stderr(); \
    }

  #define xassert(EXPRESSION) \
    if (!(EXPRESSION)) { \
      throw AssertionError() << "Assertion '" #EXPRESSION "' failed in " \
          << __FILE__ << ", line " << __LINE__; \
    }
#else
  #define wassert(EXPRESSION)
  #define xassert(EXPRESSION)
#endif


// XAssert() macro is similar to xassert(), except it works both in
// debug and in production.
#define XAssert(EXPRESSION) \
  if (!(EXPRESSION)) { \
    throw AssertionError() << "Assertion '" #EXPRESSION "' failed in " \
        << __FILE__ << ", line " << __LINE__; \
  }



#ifdef static_assert
  #define dt_static_assert static_assert
#else
  // Some system libraries fail to define this macro :( At least 3 people have
  // reported having this problem on the first day of testing, so it's not
  // that uncommon.
  // In this case we just disable the checks.
  #define dt_static_assert(x, y)
#endif



#ifdef NDEBUG
  template <typename T>
  inline void assert_compatible_type(SType) {}

#else
  template<typename T>
  inline void assert_compatible_type(SType) {
    throw NotImplError() << "Invalid type T in assert_compatible_type()";
  }

  template<>
  inline void assert_compatible_type<int8_t>(SType s) {
    xassert(s == SType::VOID || s == SType::INT8 || s == SType::BOOL);
  }

  template<>
  inline void assert_compatible_type<int16_t>(SType s) {
    xassert(s == SType::INT16);
  }

  template<>
  inline void assert_compatible_type<int32_t>(SType s) {
    xassert(s == SType::INT32);
  }

  template<>
  inline void assert_compatible_type<int64_t>(SType s) {
    xassert(s == SType::INT64);
  }

  template<>
  inline void assert_compatible_type<float>(SType s) {
    xassert(s == SType::FLOAT32);
  }

  template<>
  inline void assert_compatible_type<double>(SType s) {
    xassert(s == SType::FLOAT64);
  }

  template<>
  inline void assert_compatible_type<CString>(SType s) {
    xassert(s == SType::STR32 || s == SType::STR64);
  }

  template<>
  inline void assert_compatible_type<py::robj>(SType s) {
    xassert(s == SType::OBJ);
  }
#endif


#endif
