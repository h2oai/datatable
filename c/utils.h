//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#ifndef dt_UTILS_h
#define dt_UTILS_h
#include <Python.h>
#include <stddef.h>
#include <stdio.h>     // vsnprintf
#include <stdint.h>
#include <stdio.h>     // vsnprintf
#include <errno.h>     // errno
#include <string.h>    // strerr
#include <memory>      // std::unique_ptr
#include <string>
#include "utils/exceptions.h"


// On Windows variables of type `size_t` cannot be printed with "%zu" in the
// `snprintf()` function. For those variables we will cast them into
// `unsigned long long int` before printing; and this #define makes it simpler.
#define llu   unsigned long long int


double wallclock(void);
const char* filesize_to_str(size_t filesize);
const char* humanize_number(size_t num);

inline std::string operator "" _s(const char* str, size_t len) {
  return std::string(str, len);
}



//==============================================================================
// Pointer macros
//==============================================================================

// Some macro hackery based on
// https://github.com/pfultz2/Cloak/wiki/C-Preprocessor-tricks,-tips,-and-idioms
#define zPRIMITIVE_CAT(a, ...) a ## __VA_ARGS__
#define zCAT(a, ...) zPRIMITIVE_CAT(a, __VA_ARGS__)
#define zIIF_0(t, ...) __VA_ARGS__
#define zIIF_1(t, ...) t
#define zCHECK_N(x, n, ...) n
#define zCHECK(...) zCHECK_N(__VA_ARGS__, 0,)
#define zIIF(c) zPRIMITIVE_CAT(zIIF_, c)
#define zTEST3_ ~,1,

// This macro expands to `t` if symbol `x` is defined and equal to 1, and to
// `f` otherwise.
#define IFDEF(x, t, f) zIIF(zCHECK(zPRIMITIVE_CAT(zTEST3_, x)))(t, f)
#define WHEN(x, t) IFDEF(x, t, )

#define APPLY(macro, arg) macro(arg)
#define STRINGIFY_(L) #L
#define STRINGIFY(x) APPLY(STRINGIFY_, x)



//==============================================================================
// Binary arithmetics
//==============================================================================

namespace dt {
template <typename T> int nlz(T x);  // Number of leading zeros

extern template int nlz(uint64_t);
extern template int nlz(uint32_t);
extern template int nlz(uint16_t);
extern template int nlz(uint8_t);
};


//==============================================================================
// Other
//==============================================================================

#define zMAKE_PRAGMA(x) _Pragma(#x);

#define IGNORE_WARNING(name) \
    zMAKE_PRAGMA(GCC diagnostic push) \
    zMAKE_PRAGMA(GCC diagnostic ignored #name)

#define RESTORE_WARNINGS() \
    zMAKE_PRAGMA(GCC diagnostic pop)


void set_value(void * __restrict__ ptr, const void * __restrict__ value,
               size_t sz, size_t count);



#ifdef __APPLE__
  #include <malloc/malloc.h>  // size_t malloc_size(const void *)
#elif defined _WIN32
  #include <malloc.h>  // size_t _msize(void *)
  #define malloc_size  _msize
#elif defined __linux__
  #include <malloc.h>  // size_t malloc_usable_size(void *) __THROW
  #define malloc_size  malloc_usable_size
#else
  #define malloc_size(p)  0
  #define MALLOC_SIZE_UNAVAILABLE
#endif

char* repr_utf8(const unsigned char* ptr0, const unsigned char* ptr1);


size_t array_size(void *ptr, size_t elemsize);


#endif
