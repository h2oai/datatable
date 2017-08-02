#ifndef dt_SORT_H
#define dt_SORT_H
#include <stddef.h>
#include <stdint.h>

void* sort_i1(int8_t *x, int32_t n, int32_t **o);
void* sort_i4(int32_t *x, int32_t n, int32_t **o);
void init_sort_functions(void);


#endif
