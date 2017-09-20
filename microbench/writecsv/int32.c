#include <stdint.h>  // int8_t, ...
#include <stdio.h>   // printf
#include <stdlib.h>  // srand, rand
#include <time.h>    // time
#include "writecsv.h"



// This form assumes that at least 11 extra bytes in the buffer are available
static void kernel0(char **pch, Column *col, int64_t row) {
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
static void kernel1(char **pch, Column *col, int64_t row) {
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
static void kernel2(char **pch, Column *col, int64_t row) {
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
static void kernel3(char **pch, Column *col, int64_t row) {
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
static void kernel4(char **pch, Column *col, int64_t row)
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


// Good old 'sprintf'
static void kernel5(char **pch, Column *col, int64_t row) {
  int32_t value = ((int32_t*) col->data)[row];
  if (value == NA_I4) return;
  *pch += sprintf(*pch, "%d", value);
}


#define NKERNELS 6
typedef void (*write_kernel)(char**, Column*, int64_t);
static write_kernel kernels[NKERNELS] = {
  //            Time to write a single value and a comma, in ns
  &kernel0,  // 68.921
  &kernel1,  // 65.480
  &kernel2,  // 57.771
  &kernel3,  // 49.834
  &kernel4,  // 68.278
  &kernel5,  // 82.974
};


//=================================================================================================
// Main
//=================================================================================================

void main_int32(int B, int64_t N)
{
  srand((unsigned) time(NULL));

  // Prepare data array
  int32_t *data = malloc(N * sizeof(int32_t));
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
  Column column = { .data = (void*)data };

  // Prepare output buffer
  // At most 11 characters per entry (e.g. '-2147483647') + 1 for a comma
  char *out = malloc((N + 1) * 12 + 1000);

  // Run the experiment
  for (int k = 0; k < NKERNELS; k++) {
    write_kernel kernel = kernels[k];

    double t0 = now();
    for (int b = 0; b < B; b++) {
      char *pch = out;
      for (int64_t i = 0; i < N; i++) {
        kernel(&pch, &column, i);
        *pch++ = ',';
      }
      *pch = 0;
    }
    double t1 = now();
    out[120] = 0;
    printf("Kernel %d: %3.3f ns  [sample: %s]\n", k, (t1-t0)*1e9/B/N, out);
  }

  // Clean up
  printf("\nRaw data: ["); for (int i=0; i < 20; i++) printf("%d,", data[i]); printf("...]\n");
  free(out);
  free(data);
}
