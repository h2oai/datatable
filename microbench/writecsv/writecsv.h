#ifndef WRITECSV_H
#define WRITECSV_H
#include <stdint.h>

extern double now();

#define NWRITERS 4
#define NA_I1 -128
#define NA_I2 -32768
#define NA_I4 INT32_MIN

typedef struct Column {
    void *data;
} Column;


void main_boolean(int B, int64_t N);
void main_int8(int B, int64_t N);
void main_int16(int B, int64_t N);
void main_int32(int B, int64_t N);

#endif
