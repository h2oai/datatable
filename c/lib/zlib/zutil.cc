/* zutil.c -- target dependent utility functions for the compression library
 * Copyright (C) 1995-2017 Jean-loup Gailly
 * For conditions of distribution and use, see copyright notice in zlib.h
 */
#include "lib/zlib/zutil.h"
namespace zlib {

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wcast-qual"
#pragma clang diagnostic ignored "-Wold-style-cast"



const char * const z_errmsg[10] = {
  (const char *)"need dictionary",      /* Z_NEED_DICT       2  */
  (const char *)"stream end",          /* Z_STREAM_END      1  */
  (const char *)"",                    /* Z_OK              0  */
  (const char *)"file error",          /* Z_ERRNO         (-1) */
  (const char *)"stream error",        /* Z_STREAM_ERROR  (-2) */
  (const char *)"data error",          /* Z_DATA_ERROR    (-3) */
  (const char *)"insufficient memory", /* Z_MEM_ERROR     (-4) */
  (const char *)"buffer error",        /* Z_BUF_ERROR     (-5) */
  (const char *)"incompatible version",/* Z_VERSION_ERROR (-6) */
  (const char *)""
};



/* exported to allow conversion of error code to string for compress() and
 * uncompress()
 */
const char* zError(int err)
{
  return ERR_MSG(err);
}

#if defined(_WIN32_WCE)
  /* The Microsoft C Run-Time Library for Windows CE doesn't have
   * errno.  We define it as a global variable to simplify porting.
   * Its value is always 0 and should not be used.
   */
  int errno = 0;
#endif

#ifndef HAVE_MEMCPY

void zmemcpy(
  Bytef* dest,
  const Bytef* source,
  uInt  len
) {
  if (len == 0) return;
  do {
    *dest++ = *source++; /* ??? to be unrolled */
  } while (--len != 0);
}



int zmemcmp(
  const Bytef* s1,
  const Bytef* s2,
  uInt  len
) {
  uInt j;

  for (j = 0; j < len; j++) {
    if (s1[j] != s2[j]) return 2*(s1[j] > s2[j])-1;
  }
  return 0;
}



void zmemzero(Bytef* dest, uInt len)
{
  if (len == 0) return;
  do {
    *dest++ = 0;  /* ??? to be unrolled */
  } while (--len != 0);
}
#endif

#ifndef Z_SOLO


#ifndef MY_ZCALLOC /* Any system without a special alloc function */



voidpf zcalloc(
  voidpf opaque,
  unsigned items,
  unsigned size
) {
  (void)opaque;
  return sizeof(uInt) > 2 ? (voidpf)malloc(items * size) :
                (voidpf)calloc(items, size);
}


void zcfree (voidpf opaque, voidpf ptr)
{
  (void)opaque;
  free(ptr);
}



#endif /* MY_ZCALLOC */

#endif /* !Z_SOLO */
#pragma clang diagnostic pop


} // namespace zlib
