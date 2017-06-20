#ifndef dt_ENCODINGS_H
#define dt_ENCODINGS_H
#include <stdint.h>


int is_valid_utf8(const unsigned char *restrict src, size_t len);

int decode_sbcs(const unsigned char *restrict src, int len,
                unsigned char *restrict dest, uint32_t *map);

#endif
