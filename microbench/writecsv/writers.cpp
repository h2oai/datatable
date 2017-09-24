#include <errno.h>   // errno
#include <stdlib.h>  // srand, rand
#include <string.h>  // strerror
#include <time.h>    // time
#include <omp.h>
#include "writers.cpp"

static const int64_t DIVS[19] = {
  1L,
  10L,
  100L,
  1000L,
  10000L,
  100000L,
  1000000L,
  10000000L,
  100000000L,
  1000000000L,
  10000000000L,
  100000000000L,
  1000000000000L,
  10000000000000L,
  100000000000000L,
  1000000000000000L,
  10000000000000000L,
  100000000000000000L,
  1000000000000000000L,
};
static void write_int64(char **pch, int64_t value) {
  if (value == 0) {
    *((*pch)++) = '0';
    return;
  }
  char *ch = *pch;
  if (value < 0) {
    *ch++ = '-';
    value = -value;
  }
  int r = (value < 1000000)? 5 : 18;
  for (; value < DIVS[r]; r--);
  for (; r; r--) {
    int d = value / DIVS[r];
    *ch++ = d + '0';
    value -= d * DIVS[r];
  }
  *ch = value + '0';
  *pch = ch + 1;
}


void kernel_fwrite(const char *filename, int64_t *data)
{
  int fd = open(filename, O_WRONLY|O_CREAT, 0666);
  if (fd == -1) {
    printf("Unable to create file %s: %s", filename, strerror(errno));
    return;
  }
}



//=================================================================================================
// Main
//
// This function tests different methods of writing to a file. The data being written is not
// relevant. We will be writing int64s, because they are roughly in the middle (time-wise) compared
// to other types (the longest is double, at 200ns).
//=================================================================================================

void test_write_methods(int B, int64_t N)
{
  // First, create the dataset
  srand(time(NULL));
  int64_t *data = new int64_t[N];
  for (int64_t i = 0; i < N; i++) {
    int64_t x = static_cast<int64_t>(rand());
    int64_t y = static_cast<int64_t>(rand());
    data[i] = (x << 32) + y;
  }

  delete[] data;
}
