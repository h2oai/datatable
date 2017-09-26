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

//-------------------------------------------------------------------------------------------------
#define F64_SIGN_MASK  0x8000000000000000u
#define F64_1em5       0x3EE4F8B588E368F1u
#define F64_1e00       0x3FF0000000000000u
#define F64_1e15       0x430c6bf526340000u

/**
 * The problem of converting a floating-point number (float64) into a string
 * can be formulated as follows (assume x is positive and normal):
 *
 *   1. First, the "input" value v is decomposed into the mantissa and the
 *      exponent parts:
 *
 *          x = F * 2^(e - 52)
 *
 *      where F is uint64, and e is int. These parts can be computed using
 *      simple bit operations on `v = reinterpret_cast<uint64>(x)`:
 *
 *          F = (v & (1<<52 - 1)) | (1<<52)
 *          e = ((v >> 52) & 0x7FF) - 0x3FF
 *
 *   2. We'd like to find integer numbers D and E such that
 *
 *          x ≈ D * 10^(E - 17)
 *
 *      where 10^17 <= D < 10^18. If such numbers are found, then producing
 *      the final string is simple, one of the following forms can be used:
 *
 *          D[0] '.' D[1:] 'e' E
 *          D[0:E] '.' D[E:]
 *          '0.' '0'{-E-1} D
 *
 *   3. Denote f = F*2^-52, and d = D*10^-17. Then 1 <= f < 2, and similarly
 *      1 <= d < 10. Therefore,
 *
 *          E = log₁₀(f) + e * log₁₀2 - log₁₀(d)
 *          E = Floor[log₁₀(f) + e * log₁₀2]
 *          E ≤ Floor[1 + e * log₁₀2]
 *
 *      This may overestimate E by 1, but ultimately it doesn't matter...
 *      Then, D can be computed as
 *
 *          D = Floor[F * 2^(e - 52) * 10^(17 - E)]
 *
 *      Ultimately, if we precompute quantities
 *
 *          Z[e] = Floor[2^64 * 2^(e - 52) * 10^(16 - Floor[e * log₁₀2])]
 *
 *      for every exponent e (there are 2046 of them), then computing D
 *      will be done simply via
 *
 *          D = (F * Z[e]) >> 64
 *
 *      where F * Z is a product of two uint64 integers.
 *
 */
static void kernel_dragonfly(char **pch, Column *col, int64_t row) {
  char *ch = *pch;
  uint64_t value = ((uint64_t*) col->data)[row];
  if (value & F64_SIGN_MASK) {
    *ch++ = '-';
    value ^= F64_SIGN_MASK;
  }

  uint64_t significand = (value & 0xFFFFFFFFFFFFFL);
  int biased_exp = static_cast<int>(value >> 52);
  if (biased_exp == 0x7FF) {
    if (significand) {
      ch[0] = 'n'; ch[1] = 'a'; ch[2] = 'n';
    } else {
      ch[0] = 'i'; ch[1] = 'n'; ch[2] = 'f';
    }
    *pch = ch + 3;
    return;
  }
  int exp = biased_exp - 0x3FF;
  if (value > F64_1em5) {

  }
}



//=================================================================================================
// Main
//=================================================================================================

BenchmarkSuite prepare_bench_double(int64_t N)
{
  double v;
  v = 1e-5; printf("1e-5 = %016lx\n", *((uint64_t*)&v));
  v = 1.0;  printf("1.0  = %016lx\n", *((uint64_t*)&v));
  v = 1e15; printf("1e15 = %016lx\n", *((uint64_t*)&v));

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
