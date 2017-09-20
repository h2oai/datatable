#include <stdint.h>  // int8_t, ...
#include <stdio.h>   // printf
#include <stdlib.h>  // srand, rand
#include <time.h>    // time
#include "writecsv.h"


static void kernel1(char **pch, Column *col, int64_t row) {
  int8_t value = ((int8_t*) col->data)[row];
  if (value != NA_I1) {
    **pch = value + '0';
    (*pch)++;
  }
}

static void kernel2(char **pch, Column *col, int64_t row) {
  int8_t value = ((int8_t*) col->data)[row];
  **pch = (char)value + '0';
  *pch += (value != NA_I1);
}

static void kernel3(char **pch, Column *col, int64_t row) {
  int8_t value = ((int8_t*) col->data)[row];
  **pch = (char)value + '0';
  *pch += (value >= 0);
}


#define NKERNELS 3
typedef void (*write_kernel)(char**, Column*, int64_t);
static write_kernel kernels[NKERNELS] = {
    &kernel1,  // 6.79  6.70  6.71  6.81  6.87  7.07  6.76  6.48
    &kernel2,  // 5.94  6.13  6.15  5.97  5.82  5.82  5.99  5.80
    &kernel3,  // 5.84  6.00  6.02  5.98  5.96  5.87  6.10  5.77
};


//=================================================================================================
// Main
//=================================================================================================

void main_boolean(int B, int64_t N)
{
  srand((unsigned) time(NULL));

  // Prepare data array
  int8_t *data = malloc(N);
  for (int64_t i = 0; i < N; i++) {
    int x = rand();
    data[i] = (x&7)==0? NA_I1 : (x&1);
  }
  Column column = { .data = (void*)data };

  // Prepare output buffer
  char *out = malloc(N * 2 + 1);

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
    }
    double t1 = now();
    out[50] = 0;
    printf("Kernel %d: %.3f ms  [sample: %s]\n", k, (t1-t0)*1000/B, out);
  }

  // Clean up
  printf("\nRaw data: ["); for (int i=0; i < 20; i++) printf("%d,", data[i]); printf("...]\n");
  free(out);
  free(data);
}
