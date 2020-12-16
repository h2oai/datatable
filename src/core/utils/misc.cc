//------------------------------------------------------------------------------
// Copyright 2018-2020 H2O.ai
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
#include <chrono>           // std::chrono::steady_clock
#include <cstring>          // std::memcpy
#include <stdint.h>
#include <type_traits>      // std::is_unsigned
#include "utils/assert.h"
#include "utils/macros.h"
#include "utils/misc.h"
namespace dt {


// The warning is spurious, because shifts triggering the warning are
// within if() conditions that get statically ignored.
DISABLE_GCC_WARNING("-Wshift-count-overflow")
// 4293: shift count negative or too big, undefined behavior
DISABLE_MSVC_WARNING(4293)
// 4333: right shift by too large amount, data loss
DISABLE_MSVC_WARNING(4333)


/**
 * Number of leading zeros.
 * This algorithm tuned for the case when the number of leading zeros is
 * expected to be large.
 * Algorithm 5-7 from Henry S. Warren "Hacker's Delight"
 */
template <typename T>
int nlz(T x) {
  T y;
  int n = sizeof(T) * 8;

  if (sizeof(T) >= 8) {
    y = x >> 32; if (y != 0) { n = n -32;  x = y; }
  }
  if (sizeof(T) >= 4) {
    y = x >> 16; if (y != 0) { n = n -16;  x = y; }
  }
  if (sizeof(T) >= 2) {
    y = x >> 8;  if (y != 0) { n = n - 8;  x = y; }
  }
  if (sizeof(T) >= 1) {
    y = x >> 4;  if (y != 0) { n = n - 4;  x = y; }
    y = x >> 2;  if (y != 0) { n = n - 2;  x = y; }
    y = x >> 1;  if (y != 0) return n - 2;
  }
  return n - static_cast<int>(x);
}


template <typename T>
int nsb(T x) {
  static_assert(std::is_unsigned<T>::value, "Wrong T in nsb<T>()");
  T y;
  int m = 0;

  if (sizeof(T) >= 8) {
    y = x >> 32; if (y) { m += 32;  x = y; }
  }
  if (sizeof(T) >= 4) {
    y = x >> 16; if (y) { m += 16;  x = y; }
  }
  if (sizeof(T) >= 2) {
    y = x >> 8;  if (y) { m += 8;  x = y; }
  }
  if (sizeof(T) >= 1) {
    y = x >> 4;  if (y) { m += 4;  x = y; }
    y = x >> 2;  if (y) { m += 2;  x = y; }
    y = x >> 1;  if (y) return m + 2;
  }
  return m + static_cast<int>(x);
}



template int nlz(uint64_t);
template int nlz(uint32_t);
template int nlz(uint16_t);
template int nlz(uint8_t);
template int nsb(uint64_t);
template int nsb(uint32_t);
template int nsb(uint16_t);
template int nsb(uint8_t);

}  // namespace dt


/**
 * Fill the array `ptr` with `count` values `value` (the value is given as a
 * pointer of size `sz`). As a special case, if `value` pointer is NULL, then
 * fill with 0xFF bytes instead.
 * This is used for filling the columns with NAs.
 */
void set_value(void* ptr, const void* value, size_t sz, size_t count) {
  if (count == 0) return;
  if (value == nullptr) {
    *static_cast<unsigned char *>(ptr) = 0xFF;
    count *= sz;
    sz = 1;
  } else {
    xassert(ptr);
    std::memcpy(ptr, value, sz);
  }
  size_t final_sz = sz * count;
  for (size_t i = sz; i < final_sz; i <<= 1) {
    size_t writesz = i < final_sz - i ? i : final_sz - i;
    void* dest = static_cast<void*>(static_cast<char*>(ptr) + i);
    std::memcpy(dest, ptr, writesz);
  }
}


/**
  * Return current value on the system timer, in seconds. This function is
  * most suitable for profiling a piece of code: difference between two
  * consecutive calls to `wallclock()` will give the time elapsed in seconds.
  */
double wallclock() {
  using T = std::chrono::time_point<std::chrono::steady_clock>;
  using D = std::chrono::duration<double>;
  static T time0 = std::chrono::steady_clock::now();
  T time1 = std::chrono::steady_clock::now();
  D diff = (time1 - time0);
  return diff.count();
}


/**
 * Helper function to print file's size in human-readable format. This will
 * produce strings such as:
 *     44.74GB (48043231704 bytes)
 *     921MB (965757797 bytes)
 *     2.206MB (2313045 bytes)
 *     38.69KB (39615 bytes)
 *     214 bytes
 *     0 bytes
 * The function returns a pointer to a static string buffer, so the caller
 * should not attempt to deallocate the buffer, or call this function from
 * multiple threads at the same time, or hold on to the value returned for
 * extended periods of time.
 */
const char* filesize_to_str(size_t fsize)
{
  #define NSUFFIXES 5
  #define BUFFSIZE 100
  static char suffixes[NSUFFIXES] = {'P', 'T', 'G', 'M', 'K'};
  static char output[BUFFSIZE];
  static const char one_byte[] = "1 byte";
  llu lsize = static_cast<llu>(fsize);
  for (int i = 0; i <= NSUFFIXES; i++) {
    int shift = (NSUFFIXES - i) * 10;
    if ((fsize >> shift) == 0) continue;
    int ndigits = 3;
    for (; ndigits >= 1; ndigits--) {
      if ((fsize >> (shift + 12 - ndigits * 3)) == 0) break;
    }
    if (ndigits == 0 || (fsize == (fsize >> shift << shift))) {
      if (i < NSUFFIXES) {
        snprintf(output, BUFFSIZE, "%llu%cB",
                 lsize >> shift, suffixes[i]);
        return output;
      }
    } else {
      snprintf(output, BUFFSIZE, "%.*f%cB",
               ndigits, static_cast<double>(fsize) / static_cast<double>(1LL << shift),
               suffixes[i]);
      return output;
    }
  }
  if (fsize == 1) return one_byte;
  snprintf(output, BUFFSIZE, "%llu bytes", lsize);
  return output;
}


const char* humanize_number(size_t num) {
  static char outputs[270];
  static int curr_out = 0;
  char* output = outputs + (curr_out++) * 27;
  if (curr_out == 10) curr_out = 0;
  int len = 0;
  if (num) {
    while (true) {
      output[len++] = '0' + static_cast<char>(num % 10);
      num /= 10;
      if (!num) break;
      if (len % 4 == 3) output[len++] = ',';
    }
    for (int i = 0; i < len/2; ++i) {
      char c = output[i];
      output[i] = output[len - 1 - i];
      output[len - 1 - i] = c;
    }
  } else {
    output[len++] = '0';
  }
  output[len] = '\0';
  return output;
}


/**
 * Return the size of the array `ptr`, or 0 if the platform doesn't allow
 * such functionality.
 */
size_t array_size(void *ptr, size_t elemsize) {
  #ifdef MALLOC_SIZE_UNAVAILABLE
    return 0;
  #else
    return ptr == nullptr? 0 : malloc_size(ptr) / elemsize;
  #endif
}


char*
repr_utf8(const unsigned char* ptr0, const unsigned char* ptr1) {
    static char buf[101];
    int i = 0;
    for (const unsigned char *ptr = ptr0; ptr < ptr1; ptr++) {
        if (*ptr >= 0x20 && *ptr < 0x7F)
            buf[i++] = static_cast<char>(*ptr);
        else {
            int8_t d0 = (*ptr) & 0xF;
            int8_t d1 = (*ptr) >> 4;
            buf[i++] = '\\';
            buf[i++] = 'x';
            buf[i++] = d1 <= 9? ('0' + d1) : ('A' + d1 - 10);
            buf[i++] = d0 <= 9? ('0' + d0) : ('A' + d0 - 10);
        }
        if (i >= 95) break;
    }
    buf[i] = '\0';
    return buf;
}



RESTORE_MSVC_WARNING(4333)
RESTORE_MSVC_WARNING(4293)
RESTORE_GCC_WARNING("-Wshift-count-overflow")
