#include <stdint.h>  // int8_t, ...
#include <stdio.h>   // printf
#include <stdlib.h>  // srand, rand
#include <string.h>  // memmove
#include <time.h>    // time
#include "writecsv.h"

static void kernel_simple(char **pch, Column *col, int64_t row)
{
  int32_t offset1 = ((int32_t*) col->data)[row];
  int32_t offset0 = abs(((int32_t*) col->data)[row - 1]);
  char *ch = *pch;

  if (offset1 < 0) return;
  if (offset0 == offset1) {
    ch[0] = '"';
    ch[1] = '"';
    *pch = ch + 2;
    return;
  }
  const char *strstart = col->strbuf + offset0;
  const char *strend = col->strbuf + offset1;
  const char *sch = strstart;
  while (sch < strend && *sch != ',' && *sch != '"' && (uint8_t)*sch >= 32) {
    *ch++ = *sch++;
  }
  if (sch < strend) {
    ch = *pch;
    sch = strstart;
    *ch++ = '"';
    while (sch < strend) {
      if (*sch == '"') *ch++ = '"';  // double the quote
      *ch++ = *sch++;
    }
    *ch++ = '"';
  }
  *pch = ch;
}


static void kernel_shortcut(char **pch, Column *col, int64_t row)
{
  int32_t offset1 = ((int32_t*) col->data)[row];
  int32_t offset0 = abs(((int32_t*) col->data)[row - 1]);
  char *ch = *pch;

  if (offset1 < 0) return;
  if (offset0 == offset1) {
    ch[0] = '"';
    ch[1] = '"';
    *pch = ch + 2;
    return;
  }
  const char *strstart = col->strbuf + offset0;
  const char *strend = col->strbuf + offset1;
  const char *sch = strstart;
  while (sch < strend) {  // ',' is 44, '"' is 32
    char c = *sch;
    if ((uint8_t)c <= (uint8_t)',' && (c == ',' || c == '"' || (uint8_t)c < 32)) break;
    *ch++ = c;
    sch++;
  }
  if (sch < strend) {
    ch = *pch;
    sch = strstart;
    *ch++ = '"';
    while (sch < strend) {
      if (*sch == '"') *ch++ = '"';  // double the quote
      *ch++ = *sch++;
    }
    *ch++ = '"';
  }
  *pch = ch;
}


static void kernel_memmove(char **pch, Column *col, int64_t row)
{
  int32_t offset1 = ((int32_t*) col->data)[row];
  int32_t offset0 = abs(((int32_t*) col->data)[row - 1]);
  char *ch = *pch;

  if (offset1 < 0) return;
  if (offset0 == offset1) {
    ch[0] = '"';
    ch[1] = '"';
    *pch = ch + 2;
    return;
  }
  const char *strstart = col->strbuf + offset0;
  const char *strend = col->strbuf + offset1;
  const char *sch = strstart;
  while (sch < strend) {  // ',' is 44, '"' is 32
    char c = *sch;
    if ((uint8_t)c <= (uint8_t)',' && (c == ',' || c == '"' || (uint8_t)c < 32)) break;
    *ch++ = c;
    sch++;
  }
  if (sch < strend) {
    // We already know that characters in the range *pch .. ch are "clean",
    // i.e. do not require any escaping. Thus, we can simple memmove them.
    ch = *pch;
    memmove(ch+1, ch, sch - strstart);
    *ch = '"';
    ch += sch - strstart + 1;
    while (sch < strend) {
      if (*sch == '"') *ch++ = '"';  // double the quote
      *ch++ = *sch++;
    }
    *ch++ = '"';
  }
  *pch = ch;
}



static void kernel_memcopy(char **pch, Column *col, int64_t row)
{
  int32_t offset1 = ((int32_t*) col->data)[row];
  int32_t offset0 = abs(((int32_t*) col->data)[row - 1]);
  char *ch = *pch;

  if (offset1 < 0) return;
  if (offset0 == offset1) {
    ch[0] = '"';
    ch[1] = '"';
    *pch = ch + 2;
    return;
  }
  const char *strstart = col->strbuf + offset0;
  const char *strend = col->strbuf + offset1;
  const char *sch = strstart;
  while (sch < strend) {  // ',' is 44, '"' is 32
    char c = *sch;
    if ((uint8_t)c <= (uint8_t)',' && (c == ',' || c == '"' || (uint8_t)c < 32)) break;
    *ch++ = c;
    sch++;
  }
  if (sch < strend) {
    ch = *pch;
    memcpy(ch+1, strstart, sch - strstart);
    *ch = '"';
    ch += sch - strstart + 1;
    while (sch < strend) {
      if (*sch == '"') *ch++ = '"';  // double the quote
      *ch++ = *sch++;
    }
    *ch++ = '"';
  }
  *pch = ch;
}


// Variant of kernel_memmove, but with \0 written to the last byte of the string
static void kernel_end0(char **pch, Column *col, int64_t row)
{
  int32_t offset1 = ((int32_t*) col->data)[row];
  int32_t offset0 = abs(((int32_t*) col->data)[row - 1]);
  char *ch = *pch;

  if (offset1 < 0) return;
  if (offset0 == offset1) {
    ch[0] = '"';
    ch[1] = '"';
    *pch = ch + 2;
    return;
  }
  char *strstart = col->strbuf + offset0;
  char *strend = col->strbuf + offset1;
  char lastc = *strend;
  *strend = '\0';
  char *sch = strstart;
  for (;;) {  // ',' is 44, '"' is 32
    char c = *sch;
    if ((uint8_t)c <= (uint8_t)',' && (c == ',' || c == '"' || (uint8_t)c < 32)) break;
    *ch++ = c;
    sch++;
  }
  if (*sch) {
    ch = *pch;
    memmove(ch+1, ch, sch - strstart);
    *ch = '"';
    ch += sch - strstart + 1;
    while (*sch) {
      if (*sch == '"') *ch++ = '"';  // double the quote
      *ch++ = *sch++;
    }
    *ch++ = '"';
  }
  *strend = lastc;
  *pch = ch;
}


// Same as kernel_memmove, but attempt to move the bytes manually
static void kernel_movemanual(char **pch, Column *col, int64_t row)
{
  int32_t offset1 = ((int32_t*) col->data)[row];
  int32_t offset0 = abs(((int32_t*) col->data)[row - 1]);
  char *ch = *pch;

  if (offset1 < 0) return;
  if (offset0 == offset1) {
    ch[0] = '"';
    ch[1] = '"';
    *pch = ch + 2;
    return;
  }
  const char *strstart = col->strbuf + offset0;
  const char *strend = col->strbuf + offset1;
  const char *sch = strstart;
  while (sch < strend) {  // ',' is 44, '"' is 32
    char c = *sch;
    if ((uint8_t)c <= (uint8_t)',' && (c == ',' || c == '"' || (uint8_t)c < 32)) break;
    *ch++ = c;
    sch++;
  }
  if (sch < strend) {
    // We already know that characters in the range *pch .. ch are "clean",
    // i.e. do not require any escaping. Thus, we can simple memmove them.
    char *tch = *pch;
    char c = *tch;
    *tch++ = '"';
    while (tch < ch) {
      char cc = *tch++;
      *tch = c;
      c = cc;
    }
    ch++;
    while (sch < strend) {
      if (*sch == '"') *ch++ = '"';  // double the quote
      *ch++ = *sch++;
    }
    *ch++ = '"';
  }
  *pch = ch;
}


// Rough adaption of fwrite's method (had to adapt to our format for storing strings)
static void kernel_fwrite(char **pch, Column *col, int64_t row)
{
  static const char sep = ',';
  static const char sep2 = '|';
  int32_t offset1 = ((int32_t*) col->data)[row];
  int32_t offset0 = abs(((int32_t*) col->data)[row - 1]);
  char *ch = *pch;

  if (offset1 < 0) return;
  const char *tt = col->strbuf + offset0;
  if (offset0 == offset1) {
    *ch++='"'; *ch++='"';
    *pch = ch;
    return;
  }
  const char *ttend = col->strbuf + offset1;
  while (tt < ttend && *tt!=sep && *tt!=sep2 && *tt!='\n' && *tt!='\r' && *tt!='"') *ch++ = *tt++;
  if (tt==ttend) {
    *pch = ch;
    return;
  }
  ch = *pch;
  *ch++ = '"';
  tt = col->strbuf + offset0;
  while (tt<ttend) {
    if (*tt=='"') *ch++ = '"';
    *ch++ = *tt++;
  }
  *ch++ = '"';
  *pch = ch;
}


// Very crude version: no auto-quoting, cannot be used as-is
static void kernel_sprintf(char **pch, Column *col, int64_t row)
{
  int32_t offset1 = ((int32_t*) col->data)[row];
  int32_t offset0 = abs(((int32_t*) col->data)[row - 1]);
  if (offset1 < 0) return;
  char savedch = col->strbuf[offset1];
  col->strbuf[offset1] = '\0';
  sprintf(*pch, "%s", col->strbuf + offset0);
  col->strbuf[offset1] = savedch;
  *pch += offset1 - offset0;
}



//=================================================================================================
// Main
//=================================================================================================

BenchmarkSuite prepare_bench_string(int64_t N)
{
  // Prepare data array
  srand((unsigned) time(NULL));
  int32_t *data = (int32_t*) malloc((N+1) * sizeof(int32_t));
  char *strdata = (char*) malloc(N * 20);  // Assume string of length 20 on average
  data[0] = -1;
  for (int64_t i = 1; i <= N; i++) {
    int x = rand();
    if ((x&7)==0) {
      data[i] = -abs(data[i-1]);
    } else {
      int len = (x&15) + 5;
      data[i] = abs(data[i-1]) + len;
      char *target = strdata + data[i] - 2;
      for (int j = 0; j < len; j++) {
        int y = rand();
        char v = (y&3)==0? ((y>>2)%10) + '0' :
                 (y&3)==1? ((y>>2)%26) + 'A' :
                           ((y>>2)%26) + 'a';
        target[-j] = (y&0xFF)>=250||v==0? '"' : v;
      }
    }
  }

  // Prepare output buffer
  char *out = (char*) calloc(1, N * 21);
  Column *column = (Column*) malloc(sizeof(Column));
  column->data = (void*)(data + 1);
  column->strbuf = strdata - 1;

  printf("data = ["); for (int i = 0; i < 20; i++) printf("%d,", data[i]); printf("...]\n");
  printf("strs = ["); for (int i = 0; i < 120; i++) printf("%c", strdata[i]); printf("]\n");
  printf("\n");

  static Kernel kernels[] = {
    { &kernel_simple,     "simple" },     // 61.661
    { &kernel_shortcut,   "shortcut" },   // 53.442
    { &kernel_memmove,    "memmove" },    // 51.105
    { &kernel_memcopy,    "memcopy" },    // 50.613
    { &kernel_end0,       "end0" },       // 52.973
    { &kernel_movemanual, "movemanual" }, // 52.620
    { &kernel_fwrite,     "fwrite" },     // 70.398
    { &kernel_sprintf,    "sprintf" },    // 76.580
    { NULL, NULL },
  };

  return {
    .column = column,
    .output = out,
    .kernels = kernels,
  };
}
