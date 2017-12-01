#include "utils.h"
#include <stdint.h>


int64_t min(int64_t a, int64_t b) { return a < b? a : b; }
int64_t max(int64_t a, int64_t b) { return a > b? a : b; }
float max_f4(float a, float b) { return a < b? b : a; }


/**
 * Number of leading zeros.
 * This algorithm tuned for the case when the number of leading zeros is
 * expected to be large.
 * Algorithm 5-7 from Henry S. Warren "Hacker's Delight"
 */
int nlz(uint32_t x)
{
  uint32_t y;
  int n = 32;
  y = x >>16;  if (y != 0) { n = n -16;  x = y; }
  y = x >> 8;  if (y != 0) { n = n - 8;  x = y; }
  y = x >> 4;  if (y != 0) { n = n - 4;  x = y; }
  y = x >> 2;  if (y != 0) { n = n - 2;  x = y; }
  y = x >> 1;  if (y != 0) return n - 2;
  return n - (int)x;
}

int nlz8(uint64_t x)
{
  uint64_t y;
  int n = 64;
  y = x >>32;  if (y != 0) { n = n -32;  x = y; }
  y = x >>16;  if (y != 0) { n = n -16;  x = y; }
  y = x >> 8;  if (y != 0) { n = n - 8;  x = y; }
  y = x >> 4;  if (y != 0) { n = n - 4;  x = y; }
  y = x >> 2;  if (y != 0) { n = n - 2;  x = y; }
  y = x >> 1;  if (y != 0) return n - 2;
  return n - (int)x;
}



/**
 * Fill the array `ptr` with `count` values `value` (the value is given as a
 * pointer of size `sz`). As a special case, if `value` pointer is NULL, then
 * fill with 0xFF bytes instead.
 * This is used for filling the columns with NAs.
 */
void set_value(void * __restrict__ ptr, const void * __restrict__ value,
               size_t sz, size_t count)
{
  if (count == 0) return;
  if (value == NULL) {
    *(unsigned char *)ptr = 0xFF;
    count *= sz;
    sz = 1;
  } else {
    memcpy(ptr, value, sz);
  }
  size_t final_sz = sz * count;
  for (size_t i = sz; i < final_sz; i <<= 1) {
    size_t writesz = i < final_sz - i ? i : final_sz - i;
    memcpy(add_ptr(ptr, i), ptr, writesz);
  }
}


/**
 * Return current value on the system timer, in seconds. This function is
 * most suitable for profiling a piece of code: difference between two
 * consecutive calls to `wallclock()` will give the time elapsed in seconds.
 */
#if defined(CLOCK_REALTIME) && !defined(DISABLE_CLOCK_REALTIME)
  #include <time.h>
  double wallclock(void) {
    struct timespec tp;
    int ret = clock_gettime(CLOCK_REALTIME, &tp);
    return ret == 0? 1.0 * tp.tv_sec + 1e-9 * tp.tv_nsec : 0;
  }
#else
  #include <sys/time.h>
  double wallclock(void) {
    struct timeval tv;
    int ret = gettimeofday(&tv, NULL);
    return ret == 0? 1.0 * tv.tv_sec + 1e-6 * tv.tv_usec : 0;
  }
#endif


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
  llu lsize = (llu) fsize;
  for (int i = 0; i <= NSUFFIXES; i++) {
    int shift = (NSUFFIXES - i) * 10;
    if ((fsize >> shift) == 0) continue;
    int ndigits = 3;
    for (; ndigits >= 1; ndigits--) {
      if ((fsize >> (shift + 12 - ndigits * 3)) == 0) break;
    }
    if (ndigits == 0 || (fsize == (fsize >> shift << shift))) {
      if (i < NSUFFIXES) {
        snprintf(output, BUFFSIZE, "%llu%cB (%llu bytes)",
                 lsize >> shift, suffixes[i], lsize);
        return output;
      }
    } else {
      snprintf(output, BUFFSIZE, "%.*f%cB (%llu bytes)",
               ndigits, (double)fsize / (1 << shift), suffixes[i], lsize);
      return output;
    }
  }
  if (fsize == 1) return one_byte;
  snprintf(output, BUFFSIZE, "%llu bytes", lsize);
  return output;
}
