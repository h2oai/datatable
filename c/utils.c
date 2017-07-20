#include <stdint.h>
#include "utils.h"

int64_t min(int64_t a, int64_t b) { return a < b? a : b; }
int64_t max(int64_t a, int64_t b) { return a > b? a : b; }
float max_f4(float a, float b) { return a < b? b : a; }


/**
 * Number of leading zeros.
 * This algorithm tuned for the case when the number of leading zeros is
 * expected to be large.
 * Algorithm 5-7 from Henry S. Warren "Hacker's Delight"
 */
unsigned int nlz(uint32_t x)
{
    uint32_t y;
    uint32_t n = 32;
    y = x >>16;  if (y != 0) { n = n -16;  x = y; }
    y = x >> 8;  if (y != 0) { n = n - 8;  x = y; }
    y = x >> 4;  if (y != 0) { n = n - 4;  x = y; }
    y = x >> 2;  if (y != 0) { n = n - 2;  x = y; }
    y = x >> 1;  if (y != 0) return n - 2;
    return n - x;
}


/**
 * Fill the array `ptr` with `count` values `value` (the value is given as a
 * pointer of size `sz`). As a special case, if `value` pointer is NULL, then
 * fill with 0xFF bytes instead.
 * This is used for filling the columns with NAs.
 */
void set_value(void * restrict ptr, const void * restrict value,
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
 * Return clone of the memory buffer `src` of size `n_bytes`. This function
 * will allocate and return new memory buffer for the copy, and it will be the
 * responsibility of the caller to free the cloned buffer.
 * If this function is unable to allocate the necessary memory range, it will
 * set an error message, and return NULL.
 */
void* clone(void *src, size_t n_bytes) {
    void *restrict copy = NULL;
    dtmalloc(copy, void, n_bytes);
    memcpy(copy, src, n_bytes);
    return copy;
}
