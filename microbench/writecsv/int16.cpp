#include <stdint.h>  // int8_t, ...
#include <stdio.h>   // printf
#include <stdlib.h>  // srand, rand
#include <time.h>    // time
#include "writecsv.h"
#include "itoa_branchlut2.h"


// This form assumes that at least 6 extra bytes in the buffer are available
static void kernel_tempwrite(char **pch, Column *col, int64_t row) {
  int16_t value = ((int16_t*) col->data)[row];
  if (value == 0) {
    *((*pch)++) = '0';
    return;
  }
  char *ch = *pch;
  if (value < 0) {
    if (value == NA_I2) return;
    *ch++ = '-';
    value = -value;
  }
  char *tch = ch + 5;
  tch[1] = '\0';
  while (value) {
    int r = value / 10;  // Should be optimized by compiler to use bit-operations
    *tch-- = (value - r*10) + '0';
    value = r;
  }
  tch++;
  if (tch == ch) {
    ch += 5;
  } else {
    while (*tch) { *ch++ = *tch++; }
  }
  *pch = ch;
}


// Best approach, so far
static const int32_t DIVS10[10] = {1, 10, 100, 1000, 10000, 100000, 1000000, 10000000, 100000000, 1000000000};
static void kernel_div10(char **pch, Column *col, int64_t row) {
  int value = ((int16_t*) col->data)[row];
  if (value == 0) {
    *((*pch)++) = '0';
    return;
  }
  char *ch = *pch;
  if (value < 0) {
    if (value == NA_I2) return;
    *ch++ = '-';
    value = -value;
  }
  int r = (value < 1000)? 2 : 4;
  for (; value < DIVS10[r]; r--);
  for (; r; r--) {
    int d = value / DIVS10[r];
    *ch++ = d + '0';
    value -= d * DIVS10[r];
  }
  *ch = value + '0';
  *pch = ch + 1;
}


// Used in fwrite.c
// This code is used here only for comparison, and is not used in the
// production in any way.
static void kernel_fwrite(char **pch, Column *col, int64_t row)
{
  int16_t value = ((int16_t*) col->data)[row];
  char *ch = *pch;
  if (value == 0) {
    *ch++ = '0';
  } else if (value == NA_I2) {
    return;
  } else {
    if (value < 0) {
      *ch++ = '-';
      value = -value;
    }
    int k = 0;
    while (value) {
      *ch++ = '0' + (value % 10);
      value /= 10;
      k++;
    }
    for (int i = k/2; i; i--) {
      char tmp = *(ch-i);
      *(ch-i) = *(ch-k+i-1);
      *(ch-k+i-1) = tmp;
    }
  }
  *pch = ch;
}


// Good old 'sprintf'
static void kernel_sprintf(char **pch, Column *col, int64_t row) {
  int16_t value = ((int16_t*) col->data)[row];
  if (value == NA_I2) return;
  *pch += sprintf(*pch, "%d", value);
}



static void kernel_branchlut2(char **pch, Column *col, int64_t row) {
  int16_t value = ((int16_t*) col->data)[row];
  if (value == NA_I2) return;
  i2toa(pch, value);
}



//=================================================================================================
// Main
//=================================================================================================

BenchmarkSuite prepare_bench_int16(int64_t N)
{
  // Prepare data array
  srand((unsigned) time(NULL));
  int16_t *data = (int16_t*) malloc(N * sizeof(int16_t));
  for (int64_t i = 0; i < N; i++) {
    int x = rand();
    data[i] = (x&15)<=1? NA_I2 :
              (x&15)==2? 0 :
              (x&15)==3? x % 100 :
              (x&15)==4? -(x % 100) :
              (x&15)==5? x % 1000 :
              (x&15)==6? -(x % 1000) :
              (x&15)<=12? x : -x;
  }

  // Prepare output buffer
  // At most 6 characters per entry (e.g. '-32000') + 1 for a comma
  char *out = (char*) malloc((N + 1) * 7 + 100);
  Column *column = (Column*) malloc(sizeof(Column));
  column->data = (void*)data;

  static Kernel kernels[] = {
    { &kernel_tempwrite,  "tempwrite" },  // 50.129
    { &kernel_div10,      "div10" },      // 34.122
    { &kernel_branchlut2, "branchlut2" }, // 35.195
    { &kernel_fwrite,     "fwrite" },     // 53.396
    { &kernel_sprintf,    "sprintf" },    // 78.904
    { NULL, NULL },
  };

  return {
    .column = column,
    .output = out,
    .kernels = kernels,
  };
}
