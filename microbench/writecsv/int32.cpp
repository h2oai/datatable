//
// TODO: try implementations at
//   http://0x80.pl/articles/sse-itoa.html
//   https://github.com/miloyip/itoa-benchmark
//

#include <stdint.h>  // int8_t, ...
#include <stdio.h>   // printf
#include <stdlib.h>  // srand, rand
#include <time.h>    // time
#include "writecsv.h"
#include "itoa_branchlut2.h"



// This form assumes that at least 11 extra bytes in the buffer are available
static void kernel_tempwrite(char **pch, Column *col, int64_t row) {
  int32_t value = ((int32_t*) col->data)[row];
  if (value == 0) {
    *((*pch)++) = '0';
    return;
  }
  char *ch = *pch;
  if (value < 0) {
    if (value == NA_I4) return;
    *ch++ = '-';
    value = -value;
  }
  char *tch = ch + 10;
  tch[1] = '\0';
  while (value) {
    int d = value % 10;
    value /= 10;
    *tch-- = d + '0';
  }
  tch++;
  if (tch == ch) {
    ch += 10;
  } else {
    while (*tch) { *ch++ = *tch++; }
  }
  *pch = ch;
}

// Same as kernel0, but avoid one division operator within the loop
static void kernel_tempwrite2(char **pch, Column *col, int64_t row) {
  int32_t value = ((int32_t*) col->data)[row];
  if (value == 0) {
    *((*pch)++) = '0';
    return;
  }
  char *ch = *pch;
  if (value < 0) {
    if (value == NA_I4) return;
    *ch++ = '-';
    value = -value;
  }
  char *tch = ch + 10;
  tch[1] = '\0';
  while (value) {
    int r = value / 10;  // Should be optimized by compiler to use bit-operations
    *tch-- = (value - r*10) + '0';
    value = r;
  }
  tch++;
  if (tch == ch) {
    ch += 10;
  } else {
    while (*tch) { *ch++ = *tch++; }
  }
  *pch = ch;
}


static const int32_t DIVS[11] = {0, 1, 10, 100, 1000, 10000, 100000, 1000000, 10000000, 100000000, 1000000000};
static void kernel_div11(char **pch, Column *col, int64_t row) {
  int32_t value = ((int32_t*) col->data)[row];
  if (value == 0) {
    *((*pch)++) = '0';
    return;
  }
  char *ch = *pch;
  if (value < 0) {
    if (value == NA_I4) return;
    *ch++ = '-';
    value = -value;
  }
  int r = (value < 100000)? 5 : 10;
  for (; value < DIVS[r]; r--);
  for (; r; r--) {
    int d = value / DIVS[r];
    *ch++ = d + '0';
    value -= d * DIVS[r];
  }
  *pch = ch;
}

// Same as kernel2, but avoid last loop iteration
static const int32_t DIVS3[10] = {1, 10, 100, 1000, 10000, 100000, 1000000, 10000000, 100000000, 1000000000};
static void kernel_div10(char **pch, Column *col, int64_t row) {
  int32_t value = ((int32_t*) col->data)[row];
  if (value == 0) {
    *((*pch)++) = '0';
    return;
  }
  char *ch = *pch;
  if (value < 0) {
    if (value == NA_I4) return;
    *ch++ = '-';
    value = -value;
  }
  int r = (value < 100000)? 4 : 9;
  for (; value < DIVS3[r]; r--);
  for (; r; r--) {
    int d = value / DIVS3[r];
    *ch++ = d + '0';
    value -= d * DIVS3[r];
  }
  *ch = value + '0';
  *pch = ch + 1;
}


// Used in fwrite.c
// This code is used here only for comparison, and is not used in the
// production in any way.
static void kernel_fwrite(char **pch, Column *col, int64_t row)
{
  int32_t value = ((int32_t*) col->data)[row];
  char *ch = *pch;
  if (value == 0) {
    *ch++ = '0';
  } else if (value == NA_I4) {
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


static void kernel_fwrite2(char **pch, Column *col, int64_t row)
{
  int32_t value = ((int32_t*) col->data)[row];
  char *ch = *pch;
  if (value == NA_I4) {
    return;
  } else {
    if (value<0) { *ch++ = '-'; value = -value; }
    char *low = ch;
    do { *ch++ = '0'+static_cast<char>(value%10); value/=10; } while (value>0);
    // inlines "reverse" function
    char *upp = ch;
    upp--;
    while (upp>low) {
      char tmp = *upp;
      *upp = *low;
      *low = tmp;
      upp--;
      low++;
    }
  }
  *pch = ch;
}


// Good old 'sprintf'
static void kernel_sprintf(char **pch, Column *col, int64_t row) {
  int32_t value = ((int32_t*) col->data)[row];
  if (value == NA_I4) return;
  *pch += sprintf(*pch, "%d", value);
}


static void kernel_hex1(char **pch, Column *col, int64_t row) {
  int32_t value = ((int32_t*) col->data)[row];
  if (value == NA_I4) return;
  char *ch = *pch;
  if (value < 0) {
    *ch++ = '-';
    value = -value;
  }
  *ch++ = '0';
  *ch++ = 'x';
  if (value == 0) {
    *ch++ = '0';
  } else {
    uint32_t uvalue = static_cast<uint32_t>(value);
    while (!(uvalue & 0xF0000000)) uvalue <<= 4;
    uint8_t byte;
    while ((byte = static_cast<uint8_t>(uvalue >> 28))) {
      *ch++ = '0' + byte + (('A' - '0' - 10) & -(byte > 9));
      uvalue <<= 4;
    }
  }
  *pch = ch;
}


static void kernel_hex2(char **pch, Column *col, int64_t row) {
  int32_t value = ((int32_t*) col->data)[row];
  if (value == NA_I4) return;
  char *ch = *pch;
  if (value < 0) {
    *ch++ = '-';
    value = -value;
  }
  *ch++ = '0';
  *ch++ = 'x';
  if (value == 0) {
    *ch++ = '0';
  } else {
    uint32_t uvalue = static_cast<uint32_t>(value);
    while (!(uvalue & 0xF0000000)) uvalue <<= 4;
    uint8_t byte;
    while ((byte = static_cast<uint8_t>(uvalue >> 28))) {
      uint8_t msb = (118 + byte) & 0x80;
      *ch++ = '0' + byte + (('A' - '0' - 10) & (msb - (msb >> 7)));
      uvalue <<= 4;
    }
  }
  *pch = ch;
}


static void kernel_branchlut2(char **pch, Column *col, int64_t row) {
  int32_t value = ((int32_t*) col->data)[row];
  if (value == NA_I4) return;
  itoa(pch, value);
}



//=================================================================================================
// Main
//=================================================================================================

BenchmarkSuite prepare_bench_int32(int64_t N)
{
  // Prepare data array
  srand((unsigned) time(NULL));
  int32_t *data = (int32_t*) malloc(N * sizeof(int32_t));
  for (int64_t i = 0; i < N; i++) {
    int x = rand();
    data[i] = (x&15)<=1? NA_I4 :
              (x&15)==2? 0 :
              (x&15)==3? x % 100 :
              (x&15)==4? -(x % 100) :
              (x&15)==5? x % 1000 :
              (x&15)==6? -(x % 1000) :
              (x&15)==7? x :
              (x&15)==8? -x :
                         x % 1000000;
  }

  // Prepare output buffer
  // At most 11 characters per entry (e.g. '-2147483647') + 1 for a comma
  char *out = (char*) malloc((N + 1) * 12 + 1000);
  Column *column = (Column*) malloc(sizeof(Column));
  column->data = (void*)data;

  static Kernel kernels[] = {
    { &kernel_tempwrite,      "tempwrite" },  // 63.155
    { &kernel_tempwrite2,     "tempwrite2" }, // 60.146
    { &kernel_div11,          "div11" },      // 54.475
    { &kernel_div10,          "div10" },      // 46.425
    { &kernel_branchlut2,     "branchlut2"},  // 33.073
    { &kernel_hex1,           "hex1" },       // 31.374
    { &kernel_hex2,           "hex2" },       // 34.289
    { &kernel_fwrite,         "fwrite" },     // 65.539
    { &kernel_fwrite2,        "fwrite2" },    // 60.129
    { &kernel_sprintf,        "sprintf" },    // 80.109
    { NULL, NULL },
  };

  return {
    .column = column,
    .output = out,
    .kernels = kernels,
  };
}
