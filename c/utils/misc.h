//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#ifndef dt_UTILS_h
#define dt_UTILS_h
#include <stddef.h>
#include <stdio.h>     // vsnprintf
#include <stdint.h>
#include <stdio.h>     // vsnprintf
#include <errno.h>     // errno
#include <string.h>    // strerr
#include <memory>      // std::unique_ptr
#include <string>
#include "utils/exceptions.h"
#include "utils/macros.h"


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


void set_value(void* ptr, const void* value, size_t sz, size_t count);



#if DT_OS_DARWIN
  #include <malloc/malloc.h>  // size_t malloc_size(const void *)
#elif DT_OS_WINDOWS
  #include <malloc.h>  // size_t _msize(void *)
  #define malloc_size  _msize
#elif DT_OS_LINUX
  #include <malloc.h>  // size_t malloc_usable_size(void *) __THROW
  #define malloc_size  malloc_usable_size
#else
  #define malloc_size(p)  0
  #define MALLOC_SIZE_UNAVAILABLE
#endif

char* repr_utf8(const unsigned char* ptr0, const unsigned char* ptr1);


size_t array_size(void *ptr, size_t elemsize);


#endif
