#include "stats.h"
#include <stdlib.h>
#include <math.h>
#include <stdio.h>

/**
 * Macro to get column indices based on a RowIndex `RI`.
 * Creates a for loop with from the following parameters:
 *
 *     `RI`:    A RowIndex pointer.
 *
 *     `NROWS`: The length of the target column. This variable will be
 *              modified to store the `RI`->length, if applicable.
 *
 *     `I`:     A variable name that will be initialized as an `int64_t` and
 *              used to store the resulting index during each pass.
 *
 *     `CODE`:  The code to be placed in the body of the for loop.
 *
 * Two variables named `__TMP_0` and `__TMP_1` will also be created; Their
 * resulting type and value is undefined.
 */
#define LOOP_OVER_ROWINDEX(I, NROWS, RI, CODE)                                  \
        for (int64_t I = 0; I < NROWS; ++I) {                                   \
            CODE                                                                \
        }

StatsMacro* make_macro_stats() {
    StatsMacro* out = (StatsMacro*) malloc(sizeof(StatsMacro));
    out->count_na = -1;
    out->mean = 0;
    out->sd = -1;
    out->min = out->max = out->sum = 0;
    return out;
}

void free_macro_stats(StatsMacro* stats) {
    free(stats);
}

void print_macro_stats(StatsMacro* self) {
    printf("min:  %lld\n"
           "max:  %lld\n"
           "sum:  %lld\n"
           "mean: %f\n"
           "sd:   %f\n"
           "na count: %lld\n",
           self->min, self->max, self->sum,
           self->mean, self->sd, self->count_na);
}

void compute_macro_stats(StatsMacro* self, const Column* col) {
    int64_t count0 = 0,
            count1 = 0;
    int8_t *data = (int8_t*) col->data;
    int64_t nrows = col->nrows;
    LOOP_OVER_ROWINDEX(i, nrows, NULL,
        switch (data[i]) {
            case 0 :
                ++count0;
                break;
            case 1 :
                ++count1;
                break;
            default :
                break;
        };
    )
    int64_t count = count0 + count1;
    double mean = count > 0 ? ((double) count1) / count : -127;
    double sd = count > 1 ? sqrt((double)count0/count * count1/(count - 1)) :
                count == 1 ? 0 : -127;
    self->min = -127;
    if (count0 > 0) self->min = 0;
    else if (count1 > 0) self->min = 1;
    self->max = self->min != -127 ? count1 > 0 : -127;
    self->sum = count1;
    self->mean = mean;
    self->sd = count > 1 ? sd : -127;
    self->count_na = col->nrows - count;
}


