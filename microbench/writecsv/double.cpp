#include <stdint.h>  // int8_t, ...
#include <stdio.h>   // printf
#include <stdlib.h>  // srand, rand
#include <time.h>    // time
#include <math.h>    // isna
#include "double_lookups.h"
#include "writecsv.h"
#include "dtoa_milo.h"


static const int32_t DIVS10[10] = {1, 10, 100, 1000, 10000, 100000, 1000000, 10000000, 100000000, 1000000000};
static void write_int32(char **pch, int32_t value) {
  if (value == 0) {
    *((*pch)++) = '0';
    return;
  }
  char *ch = *pch;
  if (value < 0) {
    if (value == NA_I4) return;
    *ch++ = '-';
    value = -value;
  }
  int r = (value < 100000)? 4 : 9;
  for (; value < DIVS10[r]; r--);
  for (; r; r--) {
    int d = value / DIVS10[r];
    *ch++ = d + '0';
    value -= d * DIVS10[r];
  }
  *ch = value + '0';
  *pch = ch + 1;
}

static void write_exponent(char **pch, int value) {
  char *ch = *pch;
  if (value < 0) {
    *ch++ = '-';
    value = -value;
  } else {
    *ch++ = '+';
  }
  if (value >= 100) {
    int d = value/100;
    *ch++ = d + '0';
    value -= d*100;
    d = value/10;
    *ch++ = d + '0';
    value -= d*10;
  } else if (value >= 10) {
    int d = value/10;
    *ch++ = d + '0';
    value -= d*10;
  }
  *ch++ = value + '0';
  *pch = ch;
}



// Quick-and-dirty approach + fallback to Grisu2 for small/large numbers
static void kernel_mixed(char **pch, Column *col, int64_t row)
{
  double value = ((double*) col->data)[row];
  char *ch = *pch;

  if (isnan(value)) return;
  if (value == 0.0) {
    *ch = '0';
    (*pch)++;
    return;
  }
  if (value < 0) {
    *ch++ = '-';
    value = -value;
  }
  // For large / small numbers fallback to Grisu2
  if (value > 1e15 || value < 1e-5) {
    int length, K;
    Grisu2(value, ch, &length, &K);
    Prettify(ch, &length, K);
    *pch = ch + length;
    return;
  }

  double frac, intval;
  frac = modf(value, &intval);
  char *ch0 = ch;
  write_int32(&ch, (int32_t)intval);

  if (frac) {
    int digits_left = 14 - (ch - ch0);
    *ch++ = '.';
    while (frac > 0 && digits_left) {
      frac *= 10;
      frac = modf(frac, &intval);
      *ch++ = (int)(intval) + '0';
      digits_left--;
    }
    if (!digits_left) {
      intval = frac*10 + 0.5;
      if (intval > 9) intval = 9;
      *ch++ = (int)(intval) + '0';
    }
  }

  *pch = ch;
}


static void kernel_altmixed(char **pch, Column *col, int64_t row)
{
  double value = ((double*) col->data)[row];
  char *ch = *pch;

  if (isnan(value)) return;
  if (value == 0.0) {
    *ch = '0';
    (*pch)++;
    return;
  }
  if (value < 0) {
    *ch++ = '-';
    value = -value;
  }
  // For large / small numbers fallback to Grisu2
  if (value > 1e15 || value < 1e-5) {
    int length, K;
    Grisu2(value, ch, &length, &K);
    memmove(ch+2, ch+1, length - 1);
    ch[1] = '.';
    ch[length + 1] = 'e';
    length += 2;
    ch += length;
    write_exponent(&ch, length + K - 3);
    *pch = ch;
    return;
  }

  double frac, intval;
  frac = modf(value, &intval);
  char *ch0 = ch;
  write_int32(&ch, (int32_t)intval);

  if (frac) {
    int digits_left = 14 - (ch - ch0);
    *ch++ = '.';
    while (frac > 0 && digits_left) {
      frac *= 10;
      frac = modf(frac, &intval);
      *ch++ = (int)(intval) + '0';
      digits_left--;
    }
    if (!digits_left) {
      intval = frac*10 + 0.5;
      if (intval > 9) intval = 9;
      *ch++ = (int)(intval) + '0';
    }
  }

  *pch = ch;
}


// Used in fwrite.c
// This code is used here only for comparison, and is not used in the
// production in any way.
static void kernel_fwrite(char **pch, Column *col, int64_t row)
{
  double value = ((double*) col->data)[row];
  char *ch = *pch;

  if (!isfinite(value)) {
    if (isnan(value)) return;
    else if (value > 0) {
      *ch++ = 'I'; *ch++ = 'n'; *ch++ = 'f';
    } else {
      *ch++ = '-'; *ch++ = 'I'; *ch++ = 'n'; *ch++ = 'f';
    }
  } else if (value == 0.0) {
    *ch++ = '0';
  } else {
    if (value < 0.0) { *ch++ = '-'; value = -value; }
    union { double d; unsigned long long ull; } u;
    u.d = value;
    unsigned long long fraction = u.ull & 0xFFFFFFFFFFFFF;
    int exponent = (int)((u.ull>>52) & 0x7FF);

    double acc = 0;
    int i = 52;
    if (fraction) {
      while ((fraction & 0xFF) == 0) { fraction >>= 8; i-=8; }
      while (fraction) {
        acc += sigparts[(((fraction&1u)^1u)-1u) & i];
        i--;
        fraction >>= 1;
      }
    }
    double y = (1.0+acc) * expsig[exponent];
    int exp = exppow[exponent];
    if (y>=9.99999999999999) { y /= 10; exp++; }
    unsigned long long l = y * SIZE_SF;

    if (l%10 >= 5) l+=10;
    l /= 10;
    if (l == 0) {
      if (*(ch-1)=='-') ch--;
      *ch++ = '0';
    } else {
      int trailZero = 0;
      while (l%10 == 0) { l /= 10; trailZero++; }
      int sf = NUM_SF - trailZero;
      if (sf==0) { sf=1; exp++; }

      int dr = sf-exp-1;
      int width=0;
      int dl0=0;
      if (dr<=0) { dl0=-dr; dr=0; width=sf+dl0; }
      else {
        if (sf>dr) width=sf+1;
        else { dl0=1; width=dr+1+dl0; }
      }
      if (width <= sf + (sf>1) + 2 + (abs(exp)>99?3:2)) {
         ch += width-1;
         if (dr) {
           while (dr && sf) { *ch--='0'+l%10; l/=10; dr--; sf--; }
           while (dr) { *ch--='0'; dr--; }
           *ch-- = '.';
         }
         while (dl0) { *ch--='0'; dl0--; }
         while (sf) { *ch--='0'+l%10; l/=10; sf--; }
         ch += width+1;
      } else {
        ch += sf;
        for (int i=sf; i>1; i--) {
          *ch-- = '0' + l%10;
          l /= 10;
        }
        if (sf == 1) ch--; else *ch-- = '.';
        *ch = '0' + l;
        ch += sf + (sf>1);
        *ch++ = 'e';
        if (exp < 0) { *ch++ = '-'; exp=-exp; }
        else { *ch++ = '+'; }
        if (exp < 100) {
          *ch++ = '0' + (exp / 10);
          *ch++ = '0' + (exp % 10);
        } else {
          *ch++ = '0' + (exp / 100);
          *ch++ = '0' + (exp / 10) % 10;
          *ch++ = '0' + (exp % 10);
        }
      }
    }
  }
  *pch = ch;
}


// Good old 'sprintf'
static void kernel_sprintf(char **pch, Column *col, int64_t row) {
  double value = ((double*) col->data)[row];
  if (isnan(value)) return;
  *pch += sprintf(*pch, "%.17g", value);
}


static void kernel_miloyip(char **pch, Column *col, int64_t row) {
  double value = ((double*) col->data)[row];
  if (isnan(value)) return;
  char *ch = *pch;

  if (value == 0) {
    *ch++ = '0';
    *pch = ch;
    return;
  }
  if (value < 0) {
    *ch++ = '-';
    value = -value;
  }
  int length, K;
  Grisu2(value, ch, &length, &K);
  Prettify(ch, &length, K);
  *pch = ch + length;
}


// Note: this prints doubles in hex format, so not directly comparable!
static char hexdigits[] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};
static void kernel_hex(char **pch, Column *col, int64_t row) {
  // Read the value as if it was double
  uint64_t value = ((uint64_t*) col->data)[row];
  char *ch = *pch;

  int exp = (int)(value >> 52);
  uint64_t sig = (value & 0xFFFFFFFFFFFFF);
  if (exp & 0x800) {
    *ch++ = '-';
    exp ^= 0x800;
  }
  if (exp == 0x7FF) {  // nan & inf
    if (sig == 0) {  // - sign was already printed, if any
      ch[0] = 'i'; ch[1] = 'n'; ch[2] = 'f';
    } else {
      ch[0] = 'n'; ch[1] = 'a'; ch[2] = 'n';
    }
    *pch = ch + 3;
    return;
  }
  ch[0] = '0';
  ch[1] = 'x';
  ch[2] = '0' + (exp != 0x000);
  ch[3] = '.';
  ch += 3 + (sig != 0);
  while (sig) {
    uint64_t r = sig & 0xF000000000000;
    *ch++ = hexdigits[r >> 48];
    sig = (sig ^ r) << 4;
  }
  if (exp) exp -= 0x3FF;
  *ch++ = 'p';
  *ch++ = '+' + (exp < 0)*('-' - '+');
  write_int32(&ch, abs(exp));
  *pch = ch;
}



//=================================================================================================
// Main
//=================================================================================================

BenchmarkSuite prepare_bench_double(int64_t N)
{
  // Prepare data array
  srand((unsigned) time(NULL));
  double *data = (double*) malloc(N * sizeof(double));
  for (int64_t i = 0; i < N; i++) {
    int t = rand();
    double x = (double)(rand()) / RAND_MAX;
    data[i] = (t&15)<=1? NA_F8 :
              (t&15)==2? 0 :
              (t&15)==3? x :
              (t&15)==4? x * 100 :
              (t&15)==5? x * 10000 :
              (t&15)==6? -x :
              (t&15)==7? -10 * x :
              (t&15)==8? -1000 * x :
              (t&15)<=12? x * pow(10, 20 + t % 100) * (1 - 2*(t&1)) :
                          x * pow(0.1, 20 + t % 100) * (1 - 2*(t&1));
  }

  // Prepare output buffer
  // At most 25 characters per entry (e.g. '-1.3456789011111343e+123') + 1 for a comma
  char *out = (char*) malloc((N + 1) * 25);
  Column *column = (Column*) malloc(sizeof(Column));
  column->data = (void*)data;

  static Kernel kernels[] = {
    { &kernel_mixed,     "mixed" },    // 208.207
    { &kernel_altmixed,  "altmixed" }, // 201.784
    { &kernel_miloyip,   "miloyip" },  // 257.480
    { &kernel_hex,       "hex" },      //  78.786
    { &kernel_fwrite,    "fwrite" },   // 366.510
    { &kernel_sprintf,   "sprintf" },  // 637.765
    { NULL, NULL },
  };

  return {
    .column = column,
    .output = out,
    .kernels = kernels,
  };
}
