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
