#include <stdint.h>  // int8_t, ...
#include <stdio.h>   // printf
#include <stdlib.h>  // srand, rand
#include <time.h>    // time
#include "writecsv.h"
#include "itoa_branchlut2.h"


static void kernel_simple(char **pch, Column *col, int64_t row) {
  int8_t d, value = ((int8_t*) col->data)[row];
  if (value != NA_I1) {
    char *ch = *pch;
    if (value < 0) { *ch++ = '-'; value = -value; }
    if (value >= 100) {
      d = value/100;
      *ch++ = d + '0';
      value -= d*100;
      d = value/10;
      *ch++ = d + '0';
      value -= d*10;
    } else if (value >= 10) {
      d = value/10;
      *ch++ = d + '0';
      value -= d*10;
    }
    *ch++ = value + '0';
    *pch = ch;
  }
}


static void kernel_range1(char **pch, Column *col, int64_t row) {
  int value = (int) ((int8_t*) col->data)[row];
  char *ch = *pch;
  if (value < 0) {
    if (value == NA_I1) return;
    *ch++ = '-';
    value = -value;
  }
  if (value >= 100) {  // the range of `value` is up to 127
    *ch++ = '1';
    int d = value/10;
    *ch++ = d - 10 + '0';
    value -= d*10;
  } else if (value >= 10) {
    int d = value/10;
    *ch++ = d + '0';
    value -= d*10;
  }
  *ch++ = value + '0';
  *pch = ch;
}


static void kernel_div(char **pch, Column *col, int64_t row) {
  int8_t d, value = ((int8_t*) col->data)[row];
  if (value != NA_I1) {
    char *ch = *pch;
    if (value < 0) { *ch++ = '-'; value = -value; }
    if ((d = value/100)) {
      *ch++ = d + '0';
      value -= d*100;
      d = value/10;
      *ch++ = d + '0';
      value -= d*10;
    } else if ((d = value/10)) {
      *ch++ = d + '0';
      value -= d*10;
    }
    *ch++ = value + '0';
    *pch = ch;
  }
}


static void kernel_divloop(char **pch, Column *col, int64_t row) {
  int8_t value = ((int8_t*) col->data)[row];
  if (value == 0) {
    *((*pch)++) = '0';
    return;
  }
  char *ch = *pch;
  if (value < 0) {
    if (value == NA_I1) return;
    *ch++ = '-';
    value = -value;
  }
  char *tch = ch + 4;
  tch[1] = '\0';
  while (value) {
    int d = value % 10;
    value /= 10;
    *tch-- = d + '0';
  }
  tch++;
  while (*tch) { *ch++ = *tch++; }
  *pch = ch;
}


// Used in fwrite.c
// Note: this function is GPLv3-licensed, so can only be used in GPL projects
static void kernel_fwrite(char **pch, Column *col, int64_t row)
{
  int8_t value = ((int8_t*) col->data)[row];
  char *ch = *pch;
  if (value == 0) {
    *ch++ = '0';
  } else if (value == NA_I1) {
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


static void kernel_sprintf(char **pch, Column *col, int64_t row) {
  int8_t value = ((int8_t*) col->data)[row];
  if (value == NA_I1) return;
  *pch += sprintf(*pch, "%d", value);
}


static void kernel_branchlut2(char **pch, Column *col, int64_t row) {
  int8_t value = ((int8_t*) col->data)[row];
  if (value == NA_I1) return;
  i1toa(pch, value);
}



//=================================================================================================
// Main
//=================================================================================================

BenchmarkSuite prepare_bench_int8(int64_t N)
{
  // Prepare data array
  srand((unsigned) time(NULL));
  int8_t *data = (int8_t*) malloc(N);
  for (int64_t i = 0; i < N; i++) {
    int x = rand();
    data[i] = (x&7)==0? NA_I1 : (x&7)==1? 0 : (int8_t) (x>>3);
  }

  // Prepare output buffer
  // At most 4 characters per entry (e.g. '-100') + 1 for a comma
  char *out = (char*) malloc(N * 5 + 1);
  Column *column = (Column*) malloc(sizeof(Column));
  column->data = (void*)data;

  static Kernel kernels[] = {
    { &kernel_simple,     "simple" },     // 19.921
    { &kernel_range1,     "range1" },     // 18.294
    { &kernel_div,        "div" },        // 25.656
    { &kernel_divloop,    "divloop" },    // 30.730
    { &kernel_branchlut2, "branchlut2" }, // 21.784
    { &kernel_fwrite,     "fwrite" },     // 32.075
    { &kernel_sprintf,    "sprintf" },    // 74.886
    { NULL, NULL },
  };

  return {
    .column = column,
    .output = out,
    .kernels = kernels,
  };
}
