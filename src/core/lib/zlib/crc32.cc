/* crc32.c -- compute the CRC-32 of a data stream
 * Copyright (C) 1995-2006, 2010, 2011, 2012, 2016 Mark Adler
 * For conditions of distribution and use, see copyright notice in zlib.h
 *
 * Thanks to Rodney Brown <rbrown64@csc.com.au> for his contribution of faster
 * CRC methods: exclusive-oring 32 bits of data at a time, and pre-computing
 * tables for updating the shift register in one step with three exclusive-ors
 * instead of four steps with four exclusive-ors.  This results in about a
 * factor of two increase in speed on a Power PC G4 (PPC7455) using gcc -O3.
 */

/*
  Note on the use of DYNAMIC_CRC_TABLE: there is no mutex or semaphore
  protection on the static variables used to control the first-use generation
  of the crc tables.  Therefore, if you #define DYNAMIC_CRC_TABLE, you should
  first call get_crc_table() to initialize the tables before allowing more than
  one thread to use crc32().

  DYNAMIC_CRC_TABLE and MAKECRCH can be #defined to write out crc32.h.f
 */
#include "lib/zlib/zutil.h"      /* for STDC and definitions */
namespace zlib {


/* Definitions for doing the crc four data bytes at a time. */
#if !defined(NOBYFOUR) && defined(Z_U4)
#  define BYFOUR
#endif
#ifdef BYFOUR
   static unsigned long crc32_little(unsigned long, const unsigned char*, z_size_t);
   static unsigned long crc32_big(unsigned long, const unsigned char*, z_size_t);
#  define TBLS 8
#else
#  define TBLS 1
#endif /* BYFOUR */


#ifdef DYNAMIC_CRC_TABLE

static volatile int crc_table_empty = 1;
static z_crc_t crc_table[TBLS][256];
static void make_crc_table OF((void));
/*
  Generate tables for a byte-wise 32-bit CRC calculation on the polynomial:
  x^32+x^26+x^23+x^22+x^16+x^12+x^11+x^10+x^8+x^7+x^5+x^4+x^2+x+1.

  Polynomials over GF(2) are represented in binary, one bit per coefficient,
  with the lowest powers in the most significant bit.  Then adding polynomials
  is just exclusive-or, and multiplying a polynomial by x is a right shift by
  one.  If we call the above polynomial p, and represent a byte as the
  polynomial q, also with the lowest power in the most significant bit (so the
  byte 0xb1 is the polynomial x^7+x^3+x+1), then the CRC is (q*x^32) mod p,
  where a mod b means the remainder after dividing a by b.

  This calculation is done using the shift-register method of multiplying and
  taking the remainder.  The register is initialized to zero, and for each
  incoming bit, x^32 is added mod p to the register if the bit is a one (where
  x^32 mod p is p+x^32 = x^26+...+1), and the register is multiplied mod p by
  x (which is shifting right by one and adding x^32 mod p if the bit shifted
  out is a one).  We start with the highest power (least significant bit) of
  q and repeat for all eight bits of q.

  The first table is simply the CRC of all possible eight bit values.  This is
  all the information needed to generate CRCs on data a byte at a time for all
  combinations of CRC register values and incoming bytes.  The remaining tables
  allow for word-at-a-time CRC calculation for both big-endian and little-
  endian machines, where a word is four bytes.
*/
static void make_crc_table()
{
  z_crc_t c;
  int n, k;
  z_crc_t poly;                       /* polynomial exclusive-or pattern */
  /* terms of polynomial defining this crc (except x^32): */
  static volatile int first = 1;      /* flag to limit concurrent making */
  static const unsigned char p[] = {0,1,2,4,5,7,8,10,11,12,16,22,23,26};

  /* See if another task is already doing this (not thread-safe, but better
     than nothing -- significantly reduces duration of vulnerability in
     case the advice about DYNAMIC_CRC_TABLE is ignored) */
  if (first) {
    first = 0;

    /* make exclusive-or pattern from polynomial (0xedb88320UL) */
    poly = 0;
    for (n = 0; n < (int)(sizeof(p)/sizeof(unsigned char)); n++)
      poly |= (z_crc_t)1 << (31 - p[n]);

    /* generate a crc for every 8-bit value */
    for (n = 0; n < 256; n++) {
      c = (z_crc_t)n;
      for (k = 0; k < 8; k++)
        c = c & 1 ? poly ^ (c >> 1) : c >> 1;
      crc_table[0][n] = c;
    }

    #ifdef BYFOUR
    /* generate crc for each value followed by one, two, and three zeros,
       and then the byte reversal of those as well as the first table */
    for (n = 0; n < 256; n++) {
      c = crc_table[0][n];
      crc_table[4][n] = ZSWAP32(c);
      for (k = 1; k < 4; k++) {
        c = crc_table[0][c & 0xff] ^ (c >> 8);
        crc_table[k][n] = c;
        crc_table[k + 4][n] = ZSWAP32(c);
      }
    }
    #endif /* BYFOUR */

    crc_table_empty = 0;
  }
  else {      /* not first */
    /* wait for the other guy to finish (not efficient, but rare) */
    while (crc_table_empty)
      ;
  }
}



/* ========================================================================
 * Tables of CRC-32s of all single-byte values, made by make_crc_table().
 */
#else /* !DYNAMIC_CRC_TABLE */
  #include "lib/zlib/crc32.h"
#endif /* DYNAMIC_CRC_TABLE */




/* ========================================================================= */
#define DO1 crc = crc_table[0][(static_cast<int>(crc) ^ (*buf++)) & 0xff] ^ (crc >> 8)
#define DO8 DO1; DO1; DO1; DO1; DO1; DO1; DO1; DO1



/* ========================================================================= */
static unsigned long crc32_z(unsigned long crc, const unsigned char* buf, z_size_t len)
{
  if (buf == Z_NULL) return 0UL;

#ifdef DYNAMIC_CRC_TABLE
  if (crc_table_empty)
    make_crc_table();
#endif /* DYNAMIC_CRC_TABLE */

  #ifdef BYFOUR
    if (sizeof(void *) == sizeof(ptrdiff_t)) {
      z_crc_t endian;

      endian = 1;
      if (*((unsigned char *)(&endian)))
        return crc32_little(crc, buf, len);
      else
        return crc32_big(crc, buf, len);
    }
  #endif /* BYFOUR */
  crc = crc ^ 0xffffffffUL;
  while (len >= 8) {
    DO8;
    len -= 8;
  }
  if (len) do {
    DO1;
  } while (--len);
  return crc ^ 0xffffffffUL;
}



/* ========================================================================= */
unsigned long crc32(unsigned long crc, const unsigned char* buf, uInt len)
{
  return crc32_z(crc, buf, len);
}

#ifdef BYFOUR

/*
   This BYFOUR code accesses the passed unsigned char * buffer with a 32-bit
   integer pointer type. This violates the strict aliasing rule, where a
   compiler can assume, for optimization purposes, that two pointers to
   fundamentally different types won't ever point to the same memory. This can
   manifest as a problem only if one of the pointers is written to. This code
   only reads from those pointers. So long as this code remains isolated in
   this compilation unit, there won't be a problem. For this reason, this code
   should not be copied and pasted into a compilation unit in which other code
   writes to the buffer that is passed to these routines.
 */



/* ========================================================================= */
#define DOLIT4 c ^= *buf4++; \
    c = crc_table[3][c & 0xff] ^ crc_table[2][(c >> 8) & 0xff] ^ \
      crc_table[1][(c >> 16) & 0xff] ^ crc_table[0][c >> 24]
#define DOLIT32 DOLIT4; DOLIT4; DOLIT4; DOLIT4; DOLIT4; DOLIT4; DOLIT4; DOLIT4



/* ========================================================================= */
static unsigned long crc32_little(
  unsigned long crc,
  const unsigned char *buf,
  z_size_t len
) {
  z_crc_t c;
  const z_crc_t *buf4;

  c = (z_crc_t)crc;
  c = ~c;
  while (len && ((ptrdiff_t)buf & 3)) {
    c = crc_table[0][(c ^ *buf++) & 0xff] ^ (c >> 8);
    len--;
  }

  buf4 = (const z_crc_t *)(const void *)buf;
  while (len >= 32) {
    DOLIT32;
    len -= 32;
  }
  while (len >= 4) {
    DOLIT4;
    len -= 4;
  }
  buf = (const unsigned char *)buf4;

  if (len) do {
    c = crc_table[0][(c ^ *buf++) & 0xff] ^ (c >> 8);
  } while (--len);
  c = ~c;
  return (unsigned long)c;
}



/* ========================================================================= */
#define DOBIG4 c ^= *buf4++; \
    c = crc_table[4][c & 0xff] ^ crc_table[5][(c >> 8) & 0xff] ^ \
      crc_table[6][(c >> 16) & 0xff] ^ crc_table[7][c >> 24]
#define DOBIG32 DOBIG4; DOBIG4; DOBIG4; DOBIG4; DOBIG4; DOBIG4; DOBIG4; DOBIG4



/* ========================================================================= */
static unsigned long crc32_big(
  unsigned long crc,
  const unsigned char *buf,
  z_size_t len
) {
  z_crc_t c;
  const z_crc_t *buf4;

  c = ZSWAP32((z_crc_t)crc);
  c = ~c;
  while (len && ((ptrdiff_t)buf & 3)) {
    c = crc_table[4][(c >> 24) ^ *buf++] ^ (c << 8);
    len--;
  }

  buf4 = (const z_crc_t *)(const void *)buf;
  while (len >= 32) {
    DOBIG32;
    len -= 32;
  }
  while (len >= 4) {
    DOBIG4;
    len -= 4;
  }
  buf = (const unsigned char *)buf4;

  if (len) do {
    c = crc_table[4][(c >> 24) ^ *buf++] ^ (c << 8);
  } while (--len);
  c = ~c;
  return (unsigned long)(ZSWAP32(c));
}

#endif /* BYFOUR */



} // namespace zlib
