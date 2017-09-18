#ifndef dt_ENCODINGS_H
#define dt_ENCODINGS_H
#include <stdint.h>


int is_valid_utf8(const unsigned char *__restrict__ src, size_t len);

int decode_sbcs(const unsigned char *__restrict__ src, int len,
                unsigned char *__restrict__ dest, uint32_t *map);

#endif
