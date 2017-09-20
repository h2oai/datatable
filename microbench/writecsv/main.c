#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <assert.h>
#include "writecsv.h"
#include "utils.h"

static const char *writer_names[NWRITERS+1] = {
    "",
    "boolean",
    "int8",
    "int16",
    "int32",
};


int main(int argc, char **argv)
{
    int A = getCmdArgInt(argc, argv, "writer", 1);
    int B = getCmdArgInt(argc, argv, "batches", 100);
    int N = getCmdArgInt(argc, argv, "n", 64);
    if (A <= 0 || A > NWRITERS) {
        printf("Invalid writer: %d  (max writers=%d)\n", A, NWRITERS);
        return 1;
    }
    printf("Writer = %d (%s)\n", A, writer_names[A]);
    printf("N batches = %d\n", B);
    printf("N rows = %d\n", N);
    printf("\n");
    int x = 150;
    int8_t y = (int8_t)x;
    int8_t z = (int8_t)(uint8_t)x;

    switch (A) {
        case 1: main_boolean(B, N); break;
        case 2: main_int8(B, N); break;
        // case 3: main_int16(B, N); break;
        case 4: main_int32(B, N); break;
    }
}
