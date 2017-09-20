#include <stdint.h>  // int8_t, ...
#include <stdio.h>   // printf
#include <stdlib.h>  // srand, rand
#include <time.h>    // time
#include "writecsv.h"


static void kernel0(char **pch, Column *col, int64_t row) {
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

static void kernel1(char **pch, Column *col, int64_t row) {
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

static void kernel2(char **pch, Column *col, int64_t row) {
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

static void kernel3(char **pch, Column *col, int64_t row) {
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
    int_fast8_t d = value % 10;
    value /= 10;
    *tch-- = d + '0';
  }
  tch++;
  while (*tch) { *ch++ = *tch++; }
  *pch = ch;
}

// Used in fwrite.c
static void kernel4(char **pch, Column *col, int64_t row)
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

static void kernel5(char **pch, Column *col, int64_t row) {
  int8_t value = ((int8_t*) col->data)[row];
  if (value == NA_I1) return;
  *pch += sprintf(*pch, "%d", value);
}


#define NKERNELS 6
typedef void (*write_kernel)(char**, Column*, int64_t);
static write_kernel kernels[NKERNELS] = {
    &kernel0,
    &kernel1,
    &kernel2,
    &kernel3,
    &kernel4,
    &kernel5,
};


//=================================================================================================
// Main
//=================================================================================================

void main_int8(int B, int64_t N)
{
  srand((unsigned) time(NULL));

  // Prepare data array
  int8_t *data = malloc(N);
  for (int64_t i = 0; i < N; i++) {
    int x = rand();
    data[i] = (x&7)==0? NA_I1 : (x&7)==1? 0 : (int8_t) (x>>3);
  }
  Column column = { .data = (void*)data };

  // Prepare output buffer
  // At most 4 characters per entry (e.g. '-100') + 1 for a comma
  char *out = malloc(N * 5 + 1);

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
    out[80] = 0;
    printf("Kernel %d: %.3f ms  [sample: %s]\n", k, (t1-t0)*1000/B, out);
  }

  // Clean up
  printf("\nRaw data: ["); for (int i=0; i < 20; i++) printf("%d,", data[i]); printf("...]\n");
  free(out);
  free(data);
}
