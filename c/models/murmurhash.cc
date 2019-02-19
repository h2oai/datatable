//------------------------------------------------------------------------------
// Slightly modified Murmur hash functions that are originally written by
// Austin Appleby and placed to the public domain, see
// https://github.com/aappleby/smhasher
//------------------------------------------------------------------------------
#include "models/murmurhash.h"
#define BIG_CONSTANT(x) (x##LLU)

/*
*  Murmur2 hash function.
*/
uint64_t hash_murmur2 (const void * key, uint64_t len, unsigned int seed) {
  const uint64_t m = 0xc6a4a7935bd1e995;
  const int r = 47;

  uint64_t h = seed ^ (len * m);

  const uint64_t * data =static_cast<const uint64_t*>(key);
  const uint64_t * end = data + (len/8);

  while(data != end) {
    uint64_t k = *data++;

    k *= m;
    k ^= k >> r;
    k *= m;

    h ^= k;
    h *= m;
  }

  const unsigned char * data2 = reinterpret_cast<const unsigned char*>(data);

  switch (len & 7) {
    case 7: h ^= uint64_t(data2[6]) << 48; [[clang::fallthrough]];
    case 6: h ^= uint64_t(data2[5]) << 40; [[clang::fallthrough]];
    case 5: h ^= uint64_t(data2[4]) << 32; [[clang::fallthrough]];
    case 4: h ^= uint64_t(data2[3]) << 24; [[clang::fallthrough]];
    case 3: h ^= uint64_t(data2[2]) << 16; [[clang::fallthrough]];
    case 2: h ^= uint64_t(data2[1]) << 8;  [[clang::fallthrough]];
    case 1: h ^= uint64_t(data2[0]);
            h *= m;
  };

  h ^= h >> r;
  h *= m;
  h ^= h >> r;

  return h;
}


/*
*  Murmur3 hash function.
*/
void hash_murmur3(const void * key, const uint64_t len, unsigned int seed, void * out) {
  const uint8_t * data = static_cast<const uint8_t*>(key);
  const uint64_t nblocks = len / 16;

  uint64_t h1 = seed;
  uint64_t h2 = seed;

  const uint64_t c1 = BIG_CONSTANT(0x87c37b91114253d5);
  const uint64_t c2 = BIG_CONSTANT(0x4cf5ad432745937f);

  //----------
  // body

  const uint64_t * blocks = reinterpret_cast<const uint64_t *>(data);

  for(uint64_t i = 0; i < nblocks; i++) {
    uint64_t k1 = getblock64(blocks,i*2+0);
    uint64_t k2 = getblock64(blocks,i*2+1);

    k1 *= c1; k1  = ROTL64(k1,31); k1 *= c2; h1 ^= k1;
    h1 = ROTL64(h1,27); h1 += h2; h1 = h1*5+0x52dce729;
    k2 *= c2; k2  = ROTL64(k2,33); k2 *= c1; h2 ^= k2;
    h2 = ROTL64(h2,31); h2 += h1; h2 = h2*5+0x38495ab5;
  }

  //----------
  // tail

  const uint8_t * tail = static_cast<const uint8_t*>(data + nblocks*16);

  uint64_t k1 = 0;
  uint64_t k2 = 0;

  switch(len & 15) {
    case 15: k2 ^= (static_cast<uint64_t>(tail[14])) << 48; [[clang::fallthrough]];
    case 14: k2 ^= (static_cast<uint64_t>(tail[13])) << 40; [[clang::fallthrough]];
    case 13: k2 ^= (static_cast<uint64_t>(tail[12])) << 32; [[clang::fallthrough]];
    case 12: k2 ^= (static_cast<uint64_t>(tail[11])) << 24; [[clang::fallthrough]];
    case 11: k2 ^= (static_cast<uint64_t>(tail[10])) << 16; [[clang::fallthrough]];
    case 10: k2 ^= (static_cast<uint64_t>(tail[ 9])) << 8; [[clang::fallthrough]];
    case  9: k2 ^= (static_cast<uint64_t>(tail[ 8])) << 0;
             k2 *= c2; k2  = ROTL64(k2,33); k2 *= c1; h2 ^= k2; [[clang::fallthrough]];

    case  8: k1 ^= (static_cast<uint64_t>(tail[ 7])) << 56; [[clang::fallthrough]];
    case  7: k1 ^= (static_cast<uint64_t>(tail[ 6])) << 48; [[clang::fallthrough]];
    case  6: k1 ^= (static_cast<uint64_t>(tail[ 5])) << 40; [[clang::fallthrough]];
    case  5: k1 ^= (static_cast<uint64_t>(tail[ 4])) << 32; [[clang::fallthrough]];
    case  4: k1 ^= (static_cast<uint64_t>(tail[ 3])) << 24; [[clang::fallthrough]];
    case  3: k1 ^= (static_cast<uint64_t>(tail[ 2])) << 16; [[clang::fallthrough]];
    case  2: k1 ^= (static_cast<uint64_t>(tail[ 1])) << 8; [[clang::fallthrough]];
    case  1: k1 ^= (static_cast<uint64_t>(tail[ 0])) << 0;
             k1 *= c1; k1  = ROTL64(k1,31); k1 *= c2; h1 ^= k1;
  };

  //----------
  // finalization

  h1 ^= static_cast<const uint64_t>(len); h2 ^= static_cast<const uint64_t>(len);

  h1 += h2;
  h2 += h1;

  h1 = fmix64(h1);
  h2 = fmix64(h2);

  h1 += h2;
  h2 += h1;

  (static_cast<uint64_t*>(out))[0] = h1;
  (static_cast<uint64_t*>(out))[1] = h2;
}


/*
* Some helper functions for Murmur3 hash
*/
inline uint64_t ROTL64 ( uint64_t x, int8_t r ) {
  return (x << r) | (x >> (64 - r));
}

/*
* Block read - if your platform needs to do endian-swapping or can only
* handle aligned reads, do the conversion here
*/
inline uint64_t getblock64(const uint64_t * p, uint64_t i ) {
  return p[i];
}


/*
* Finalization mix - force all bits of a hash block to avalanche
*/
inline uint64_t fmix64 ( uint64_t k ) {
  k ^= k >> 33;
  k *= BIG_CONSTANT(0xff51afd7ed558ccd);
  k ^= k >> 33;
  k *= BIG_CONSTANT(0xc4ceb9fe1a85ec53);
  k ^= k >> 33;

  return k;
}
