//------------------------------------------------------------------------------
// Slightly modified Murmur hash functions that are originally written by
// Austin Appleby and placed to the public domain, see
// https://github.com/aappleby/smhasher
//------------------------------------------------------------------------------
#include "types.h"

uint64_t hash_murmur2(const void *, uint64_t);
void hash_murmur3(const void *, uint64_t, void *);

uint64_t ROTL64(uint64_t, int8_t);
uint64_t getblock64 (const uint64_t *, uint64_t);
uint64_t fmix64 (uint64_t);
