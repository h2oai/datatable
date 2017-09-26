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
#define F64_MANT_MASK  0x000FFFFFFFFFFFFFu
#define F64_EXTRA_BIT  0x0010000000000000u
#define F64_1em5       0x3EE4F8B588E368F1u
#define F64_1e00       0x3FF0000000000000u
#define F64_1e15       0x430c6bf526340000u
#define TENp17  100000000000000000
#define TENp18  1000000000000000000
static const int64_t DIVS64[19] = {
  1L,
  10L,
  100L,
  1000L,
  10000L,
  100000L,
  1000000L,
  10000000L,
  100000000L,
  1000000000L,
  10000000000L,
  100000000000L,
  1000000000000L,
  10000000000000L,
  100000000000000L,
  1000000000000000L,
  10000000000000000L,
  100000000000000000L,
  1000000000000000000L,
};

/**
 * The problem of converting a floating-point number (float64) into a string
 * can be formulated as follows (assume x is positive and normal):
 *
 *   1. First, the "input" value v is decomposed into the mantissa and the
 *      exponent parts:
 *
 *          x = f * 2^e = F * 2^(e - 52)
 *
 *      where F is uint64, and e is int. These parts can be computed using
 *      simple bit operations on `v = reinterpret_cast<uint64>(x)`:
 *
 *          F = (v & (1<<52 - 1)) | (1<<52)
 *          e = ((v >> 52) & 0x7FF) - 0x3FF
 *
 *   2. We'd like to find integer numbers D and E such that
 *
 *          x ≈ d * 10^E = D * 10^(E - 17)
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
 *      This may overestimate E by 1, but ultimately it doesn't matter... In
 *      practice we can replace this formula with one that is close numerically
 *      but easier to compute:
 *
 *          E = Floor[(e*1233 - 8)/4096] = ((201 + eb*1233)>>12) - 308
 *
 *      where eb = e + 0x3FF is the biased exponent.
 *
 *   4. Then, D can be computed as
 *
 *          D = Floor[F * 2^(e - 52) * 10^(17 - E(e))]
 *
 *      (with the choice of E(e) as above, this quantity will range from
 *      1.001e17 to 2.015e18. The coefficients in E(e) were optimized in order
 *      to minimize sup(D) subject to inf(D)>=1e17.)
 *
 *      In this expression, F is integer, whereas 2^(e-52) * 10^(17-E(e)) is
 *      float. If we write the latter as (A+B)/2^53 (where B < 1), then
 *      expression for D becomes
 *
 *          D = Floor[(F * A)/2^53 + (F * B)/2^53]
 *
 *      Here F<2^53 and B<1, and therefore the second term is negligible.
 *      Thus,
 *
 *         D = (F * A(e)) >> 53
 *         A(e) = Floor[2^(e+1) * 10^(17-E(e))]
 *
 *      The quantities A(e) are `uint64`s in the range approximately from
 *      2.002e17 to 2.015e18. They can be precomputed and stored for every
 *      exponent e (there are 2046 of them). Multiplying and shifting of
 *      two uint64 numbers is fast and simple.
 *
 * This algorithm is roughly similar to Grisu2.
 */
static uint64_t Atable[2048] = {
  0x03168149dd886f8a, 0x062d0293bb10df15, 0x0c5a05277621be29, 0x18b40a4eec437c52,
  0x04f0cedc95a718dd, 0x09e19db92b4e31bb, 0x13c33b72569c6375, 0x03f3d8b077b8e0b1,
  0x07e7b160ef71c162, 0x0fcf62c1dee382c4, 0x03297a26c62d808e, 0x0652f44d8c5b011b,
  0x0ca5e89b18b60237, 0x194bd136316c046d, 0x050f29d7a37c00e3, 0x0a1e53af46f801c5,
  0x143ca75e8df0038a, 0x040c21794f96671c, 0x081842f29f2cce37, 0x103085e53e599c6f,
  0x033ce7943fab85b0, 0x0679cf287f570b5f, 0x0cf39e50feae16bf, 0x19e73ca1fd5c2d7e,
  0x052e3f5399126f80, 0x0a5c7ea73224deff, 0x14b8fd4e6449bdfe, 0x0424ff76140ebf99,
  0x0849feec281d7f33, 0x1093fdd8503afe65, 0x0350cc5e767232e1, 0x06a198bcece465c2,
  0x0d433179d9c8cb84, 0x1a8662f3b3919708, 0x054e13ca571d1e35, 0x0a9c2794ae3a3c6a,
  0x15384f295c7478d3, 0x043e763b78e4182a, 0x087cec76f1c83055, 0x10f9d8ede39060a9,
  0x03652b62c71ce022, 0x06ca56c58e39c044, 0x0d94ad8b1c738087, 0x1b295b1638e7010f,
  0x056eabd13e9499d0, 0x0add57a27d29339f, 0x15baaf44fa52673f, 0x0458897432107b0d,
  0x08b112e86420f619, 0x116225d0c841ec32, 0x037a0790280d2f3d, 0x06f40f20501a5e7a,
  0x0de81e40a034bcf5, 0x02c8060cecd758fe, 0x05900c19d9aeb1fc, 0x0b201833b35d63f7,
  0x1640306766bac7ee, 0x04733ce17af227fc, 0x08e679c2f5e44ff9, 0x11ccf385ebc89ff2,
  0x038f63e7958e8664, 0x071ec7cf2b1d0cc7, 0x0e3d8f9e563a198e, 0x02d91cb94472051c,
  0x05b2397288e40a39, 0x0b6472e511c81472, 0x16c8e5ca239028e4, 0x048e945ba0b66e94,
  0x091d28b7416cdd28, 0x123a516e82d9ba50, 0x03a5437c8091f210, 0x074a86f90123e420,
  0x0e950df20247c840, 0x02ea9c639a0e5b40, 0x05d538c7341cb680, 0x0baa718e68396d00,
  0x1754e31cd072da00, 0x04aa93d29016f866, 0x095527a5202df0cd, 0x12aa4f4a405be199,
  0x03bba97540126052, 0x077752ea8024c0a4, 0x0eeea5d500498148, 0x02fc8791000eb375,
  0x05f90f22001d66e9, 0x0bf21e44003acdd3, 0x17e43c8800759ba6, 0x04c73f4e667debee,
  0x098e7e9cccfbd7dc, 0x131cfd3999f7afb8, 0x03d2990b8531898b, 0x07a532170a631316,
  0x0f4a642e14c6262d, 0x030ee0d60427a13c, 0x061dc1ac084f4278, 0x0c3b8358109e84f0,
  0x187706b0213d09e1, 0x04e49af006a5cec7, 0x09c935e00d4b9d8d, 0x13926bc01a973b1a,
  0x03ea158cd21e3f05, 0x07d42b19a43c7e0b, 0x0fa856334878fc15, 0x0321aad70e7e98d1,
  0x064355ae1cfd31a2, 0x0c86ab5c39fa6344, 0x190d56b873f4c688, 0x0502aaf1b0ca8e1b,
  0x0a0555e361951c36, 0x140aabc6c32a386d, 0x0402225af3d53e7c, 0x080444b5e7aa7cf8,
  0x1008896bcf54f9f1, 0x0334e848c310feca, 0x0669d0918621fd93, 0x0cd3a1230c43fb27,
  0x19a742461887f64e, 0x052173a79e8197a9, 0x0a42e74f3d032f52, 0x1485ce9e7a065ea5,
  0x041ac2ec7ece12ee, 0x083585d8fd9c25db, 0x106b0bb1fb384bb7, 0x03489bf06571a8be,
  0x069137e0cae3517c, 0x0d226fc195c6a2f9, 0x1a44df832b8d45f2, 0x0540f980a24f7464,
  0x0a81f301449ee8c7, 0x1503e602893dd18e, 0x0433facd4ea5f6b6, 0x0867f59a9d4bed6c,
  0x10cfeb353a97dad8, 0x035cc8a43eeb2bc5, 0x06b991487dd6578a, 0x0d732290fbacaf13,
  0x1ae64521f7595e26, 0x05614106cb11dfa1, 0x0ac2820d9623bf43, 0x1585041b2c477e85,
  0x044dcd9f08db194e, 0x089b9b3e11b6329c, 0x1137367c236c6537, 0x0371714c0715add8,
  0x06e2e2980e2b5bb0, 0x0dc5c5301c56b75f, 0x1b8b8a6038ad6ebf, 0x05824ee00b55e2f3,
  0x0b049dc016abc5e6, 0x16093b802d578bcc, 0x04683f19a2ab1bf6, 0x08d07e33455637eb,
  0x11a0fc668aac6fd6, 0x038698e14eef4991, 0x070d31c29dde9323, 0x0e1a63853bbd2645,
  0x02d213e7725907a7, 0x05a427cee4b20f4f, 0x0b484f9dc9641e9e, 0x16909f3b92c83d3b,
  0x0483530bea280c3f, 0x0906a617d450187e, 0x120d4c2fa8a030fc, 0x039c426fee867032,
  0x073884dfdd0ce065, 0x0e7109bfba19c0ca, 0x02e368598b9ec028, 0x05c6d0b3173d8051,
  0x0b8da1662e7b00a1, 0x171b42cc5cf60143, 0x049f0d5c129799da, 0x093e1ab8252f33b4,
  0x127c35704a5e6769, 0x03b27116754614af, 0x0764e22cea8c295d, 0x0ec9c459d51852ba,
  0x02f527452a9e76f2, 0x05ea4e8a553cede4, 0x0bd49d14aa79dbc8, 0x17a93a2954f3b790,
  0x04bb72084430be50, 0x0976e41088617ca0, 0x12edc82110c2f940, 0x03c928069cf3cb73,
  0x0792500d39e796e6, 0x0f24a01a73cf2dcd, 0x030753387d8fd5f6, 0x060ea670fb1fabec,
  0x0c1d4ce1f63f57d7, 0x183a99c3ec7eafae, 0x04d885272f4c8989, 0x09b10a4e5e991313,
  0x1362149cbd322625, 0x03e06a85bf706e07, 0x07c0d50b7ee0dc0f, 0x0f81aa16fdc1b81e,
  0x0319eed165f38b39, 0x0633dda2cbe71672, 0x0c67bb4597ce2ce5, 0x18cf768b2f9c59c9,
  0x04f64ae8a31f4528, 0x09ec95d1463e8a50, 0x13d92ba28c7d14a1, 0x03f83bed4f4c3753,
  0x07f077da9e986ea7, 0x0fe0efb53d30dd4d, 0x032cfcbdd909c5dc, 0x0659f97bb2138bb9,
  0x0cb3f2f764271771, 0x1967e5eec84e2ee2, 0x0514c796280fa2fa, 0x0a298f2c501f45f4,
  0x14531e58a03e8be8, 0x04109fab533fb595, 0x08213f56a67f6b2a, 0x10427ead4cfed653,
  0x03407fbc42995e11, 0x0680ff788532bc21, 0x0d01fef10a657843, 0x1a03fde214caf086,
  0x0533ff939dc2301b, 0x0a67ff273b846035, 0x14cffe4e7708c06b, 0x04299942e49b59af,
  0x08533285c936b35e, 0x10a6650b926d66bc, 0x03547a9bea15e159, 0x06a8f537d42bc2b2,
  0x0d51ea6fa8578563, 0x1aa3d4df50af0ac6, 0x0553f75fdcefcef4, 0x0aa7eebfb9df9de9,
  0x154fdd7f73bf3bd2, 0x04432c4cb0bfd8c4, 0x08865899617fb187, 0x110cb132c2ff630e,
  0x0368f03d5a3313d0, 0x06d1e07ab466279f, 0x0da3c0f568cc4f3f, 0x1b4781ead1989e7d,
  0x0574b3955d1e8619, 0x0ae9672aba3d0c32, 0x15d2ce55747a1864, 0x045d5c777db204e1,
  0x08bab8eefb6409c2, 0x117571ddf6c81383, 0x037de392caf4d0b4, 0x06fbc72595e9a168,
  0x0df78e4b2bd342cf, 0x02cb1c756f2a4090, 0x059638eade548120, 0x0b2c71d5bca90240,
  0x1658e3ab7952047f, 0x04782d88b1dd3419, 0x08f05b1163ba6833, 0x11e0b622c774d066,
  0x039357a08e4a9014, 0x0726af411c952029, 0x0e4d5e82392a4051, 0x02dc461a0b6ed9aa,
  0x05b88c3416ddb354, 0x0b7118682dbb66a7, 0x16e230d05b76cd4f, 0x0493a35cdf17c2a9,
  0x092746b9be2f8553, 0x124e8d737c5f0aa6, 0x03a94f7d7f463554, 0x07529efafe8c6aa9,
  0x0ea53df5fd18d551, 0x02edd931329e9110, 0x05dbb262653d2220, 0x0bb764c4ca7a4441,
  0x176ec98994f48882, 0x04afc1e850fdb4e7, 0x095f83d0a1fb69ce, 0x12bf07a143f6d39b,
  0x03bfce5373fe2a52, 0x077f9ca6e7fc54a4, 0x0eff394dcff8a949, 0x02ffd842c331bb75,
  0x05ffb085866376ea, 0x0bff610b0cc6edd4, 0x17fec216198ddba8, 0x04cc8d379eb5f8bb,
  0x09991a6f3d6bf176, 0x133234de7ad7e2ed, 0x03d6d75fb22b2d63, 0x07adaebf64565ac5,
  0x0f5b5d7ec8acb58a, 0x031245e628228ab5, 0x06248bcc5045156a, 0x0c491798a08a2ad5,
  0x18922f31411455aa, 0x04ea097040374455, 0x09d412e0806e88aa, 0x13a825c100dd1155,
  0x03ee6df366929d11, 0x07dcdbe6cd253a22, 0x0fb9b7cd9a4a7444, 0x032524c2b8754a74,
  0x064a498570ea94e8, 0x0c94930ae1d529d0, 0x19292615c3aa53a0, 0x05083ad1272210ba,
  0x0a1075a24e442173, 0x1420eb449c8842e6, 0x040695741f4e73c8, 0x080d2ae83e9ce78f,
  0x101a55d07d39cf1e, 0x03387790190b8fd3, 0x0670ef2032171fa6, 0x0ce1de40642e3f4c,
  0x19c3bc80c85c7e97, 0x052725b35b45b2eb, 0x0a4e4b66b68b65d6, 0x149c96cd6d16cbac,
  0x041f515c49048f22, 0x083ea2b892091e45, 0x107d457124123c8a, 0x034c4116a0d07282,
  0x0698822d41a0e504, 0x0d31045a8341ca08, 0x1a6208b506839410, 0x0546ce8a9ae71d9d,
  0x0a8d9d1535ce3b39, 0x151b3a2a6b9c7673, 0x0438a53baf1f4ae4, 0x08714a775e3e95c8,
  0x10e294eebc7d2b8f, 0x0360842fbf4c3be9, 0x06c1085f7e9877d3, 0x0d8210befd30efa6,
  0x1b04217dfa61df4b, 0x056739e5fee05fdc, 0x0ace73cbfdc0bfb8, 0x159ce797fb817f6f,
  0x045294b7ff19e649, 0x08a5296ffe33cc93, 0x114a52dffc679926, 0x037543c665ae51d4,
  0x06ea878ccb5ca3a9, 0x0dd50f1996b94752, 0x1baa1e332d728ea3, 0x05886c70a2b082ed,
  0x0b10d8e1456105db, 0x1621b1c28ac20bb6, 0x046d238d4ef39bf1, 0x08da471a9de737e2,
  0x11b48e353bce6fc5, 0x038a82d7725c7cc1, 0x071505aee4b8f982, 0x0e2a0b5dc971f304,
  0x02d535792849fd67, 0x05aa6af25093face, 0x0b54d5e4a127f59d, 0x16a9abc9424feb39,
  0x0488558ea6dcc8a5, 0x0910ab1d4db9914a, 0x1221563a9b732294, 0x03a044721f1706ea,
  0x074088e43e2e0dd5, 0x0e8111c87c5c1baa, 0x02e69d2818df38bc, 0x05cd3a5031be7177,
  0x0b9a74a0637ce2ee, 0x1734e940c6f9c5dc, 0x04a42ea68e31f45f, 0x09485d4d1c63e8be,
  0x1290ba9a38c7d17d, 0x03b68bb871c1904c, 0x076d1770e3832098, 0x0eda2ee1c7064131,
  0x02f86fc6c167a6a3, 0x05f0df8d82cf4d47, 0x0be1bf1b059e9a8d, 0x17c37e360b3d351b,
  0x04c0b2d79bd90a9f, 0x098165af37b2153e, 0x1302cb5e6f642a7c, 0x03cd5bdfafe0d54c,
  0x079ab7bf5fc1aa98, 0x0f356f7ebf835530, 0x030aafe6264d7770, 0x06155fcc4c9aeee0,
  0x0c2abf989935ddc0, 0x18557f31326bbb80, 0x04dde63d0a158be6, 0x09bbcc7a142b17cd,
  0x137798f428562f99, 0x03e4b830d4de0985, 0x07c97061a9bc130a, 0x0f92e0c353782614,
  0x031d602710b1a137, 0x063ac04e2163426f, 0x0c75809c42c684dd, 0x18eb0138858d09ba,
  0x04fbcd0b4de901f2, 0x09f79a169bd203e4, 0x13ef342d37a407c8, 0x03fca4090b20ce5b,
  0x07f9481216419cb6, 0x0ff290242c83396d, 0x0330833a6f4d71e3, 0x06610674de9ae3c5,
  0x0cc20ce9bd35c78a, 0x198419d37a6b8f15, 0x051a6b90b2158304, 0x0a34d721642b0608,
  0x1469ae42c8560c11, 0x041522da2811359d, 0x082a45b450226b3a, 0x10548b68a044d674,
  0x03441be1b9a75e17, 0x068837c3734ebc2e, 0x0d106f86e69d785c, 0x1a20df0dcd3af0b9,
  0x0539c635f5d8968b, 0x0a738c6bebb12d17, 0x14e718d7d7625a2e, 0x042e382b2b13aba3,
  0x085c705656275745, 0x10b8e0acac4eae8b, 0x03582cef55a9561c, 0x06b059deab52ac38,
  0x0d60b3bd56a5586f, 0x1ac1677aad4ab0de, 0x0559e17eef755693, 0x0ab3c2fddeeaad26,
  0x156785fbbdd55a4b, 0x0447e798bf91120f, 0x088fcf317f22241e, 0x111f9e62fe44483c,
  0x036cb946ffa741a6, 0x06d9728dff4e834b, 0x0db2e51bfe9d0697, 0x1b65ca37fd3a0d2d,
  0x057ac20b32a535d6, 0x0af58416654a6bac, 0x15eb082cca94d757, 0x046234d5c21dc4ab,
  0x08c469ab843b8956, 0x1188d357087712ac, 0x0381c3de34e49d56, 0x070387bc69c93aab,
  0x0e070f78d3927557, 0x02ce364b5d83b111, 0x059c6c96bb076223, 0x0b38d92d760ec445,
  0x1671b25aec1d888b, 0x047d23abc8d2b4e9, 0x08fa475791a569d1, 0x11f48eaf234ad3a2,
  0x03974fbca0a890ba, 0x072e9f7941512174, 0x0e5d3ef282a242e8, 0x02df72fd4d53a6fb,
  0x05bee5fa9aa74df6, 0x0b7dcbf5354e9bed, 0x16fb97ea6a9d37da, 0x0498b7fbaeec3e5f,
  0x09316ff75dd87cbe, 0x1262dfeebbb0f97b, 0x03ad5ffc8bf031e5, 0x075abff917e063cb,
  0x0eb57ff22fc0c796, 0x02f11996d659c184, 0x05e2332dacb38309, 0x0bc4665b59670611,
  0x1788ccb6b2ce0c23, 0x04b4f5be23c2cf3a, 0x0969eb7c47859e74, 0x12d3d6f88f0b3ce8,
  0x03c3f7cb4fcf0c2e, 0x0787ef969f9e185d, 0x0f0fdf2d3f3c30ba, 0x03032ca2a63f3cf2,
  0x060659454c7e79e4, 0x0c0cb28a98fcf3c8, 0x1819651531f9e790, 0x04d1e1043d31fb1d,
  0x09a3c2087a63f63a, 0x13478410f4c7ec73, 0x03db1a69ca8e627d, 0x07b634d3951cc4fb,
  0x0f6c69a72a3989f6, 0x0315aebb0871e864, 0x062b5d7610e3d0c9, 0x0c56baec21c7a191,
  0x18ad75d8438f4323, 0x04ef7df80d830d6d, 0x09defbf01b061adb, 0x13bdf7e0360c35b5,
  0x03f2cb2cd79c0abe, 0x07e59659af38157c, 0x0fcb2cb35e702af8, 0x0328a28a46166efe,
  0x065145148c2cddfd, 0x0ca28a291859bbf9, 0x1945145230b377f2, 0x050dd0dd3cf0b197,
  0x0a1ba1ba79e1632e, 0x14374374f3c2c65c, 0x040b0d7dca5a27ac, 0x08161afb94b44f58,
  0x102c35f729689eb0, 0x033c0acb08481fbd, 0x0678159610903f79, 0x0cf02b2c21207ef3,
  0x19e056584240fde6, 0x052cde11a6d9cc61, 0x0a59bc234db398c2, 0x14b378469b673185,
  0x0423e4daebe1704e, 0x0847c9b5d7c2e09b, 0x108f936baf85c137, 0x034fea48bcb459d8,
  0x069fd4917968b3b0, 0x0d3fa922f2d1675f, 0x1a7f5245e5a2cebe, 0x054caa0dfaba2959,
  0x0a99541bf57452b3, 0x1532a837eae8a565, 0x043d54d7fbc82114, 0x087aa9aff7904228,
  0x10f5535fef208451, 0x036443dffca01a77, 0x06c887bff94034ed, 0x0d910f7ff28069da,
  0x1b221effe500d3b5, 0x056d396661002a57, 0x0ada72ccc20054af, 0x15b4e5998400a95d,
  0x0457611eb40021df, 0x08aec23d680043bf, 0x115d847ad000877e, 0x03791a7ef6668180,
  0x06f234fdeccd02ff, 0x0de469fbd99a05fe, 0x02c7486591eb9acc, 0x058e90cb23d73599,
  0x0b1d219647ae6b32, 0x163a432c8f5cd664, 0x04720d6f4fdf5e14, 0x08e41ade9fbebc28,
  0x11c835bd3f7d7850, 0x038e7125d97f7e76, 0x071ce24bb2fefced, 0x0e39c49765fdf9d9,
  0x02d85a84adff985f, 0x05b0b5095bff30bd, 0x0b616a12b7fe617b, 0x16c2d4256ffcc2f5,
  0x048d5da11665c097, 0x091abb422ccb812f, 0x123576845997025e, 0x03a44ae7451e33ac,
  0x074895ce8a3c6759, 0x0e912b9d1478ceb1, 0x02e9d585d0e4f623, 0x05d3ab0ba1c9ec47,
  0x0ba756174393d88e, 0x174eac2e8727b11c, 0x04a955a2e7d4bd06, 0x0952ab45cfa97a0b,
  0x12a5568b9f52f416, 0x03baaae8b976fd9e, 0x077555d172edfb3c, 0x0eeaaba2e5dbf678,
  0x02fbbbed612bfe18, 0x05f777dac257fc30, 0x0beeefb584aff860, 0x17dddf6b095ff0c0,
  0x04c5f97bceacc9c0, 0x098bf2f79d599380, 0x1317e5ef3ab32700, 0x03d194630bbd6e33,
  0x07a328c6177adc67, 0x0f46518c2ef5b8cd, 0x030e104f3c978b5c, 0x061c209e792f16b8,
  0x0c38413cf25e2d71, 0x18708279e4bc5ae2, 0x04e34d4b9425abc7, 0x09c69a97284b578d,
  0x138d352e5096af1b, 0x03e90aa2dceaefd2, 0x07d21545b9d5dfa4, 0x0fa42a8b73abbf49,
  0x0320d54f17225975, 0x0641aa9e2e44b2ea, 0x0c83553c5c8965d4, 0x1906aa78b912cba8,
  0x0501554b5836f588, 0x0a02aa96b06deb10, 0x1405552d60dbd620, 0x04011109135f2ad3,
  0x0802221226be55a6, 0x100444244d7cab4d, 0x03340da0dc4c2243, 0x06681b41b8984485,
  0x0cd036837130890a, 0x19a06d06e2611214, 0x052015ce2d469d37, 0x0a402b9c5a8d3a6e,
  0x14805738b51a74dd, 0x0419ab0b576bb0f9, 0x08335616aed761f2, 0x1066ac2d5daec3e4,
  0x0347bc0912bc8d94, 0x068f781225791b28, 0x0d1ef0244af23650, 0x1a3de04895e46ca0,
  0x053f9341b79415ba, 0x0a7f26836f282b73, 0x14fe4d06de5056e6, 0x0432dc3492dcde2e,
  0x0865b86925b9bc5c, 0x10cb70d24b7378b8, 0x035be35d424a4b58, 0x06b7c6ba849496b0,
  0x0d6f8d7509292d60, 0x1adf1aea12525ac0, 0x055fd22ed076def3, 0x0abfa45da0edbde7,
  0x157f48bb41db7bcd, 0x044ca82573924bf6, 0x0899504ae72497ec, 0x1132a095ce492fd7,
  0x037086845c750991, 0x06e10d08b8ea1323, 0x0dc21a1171d42646, 0x1b843422e3a84c8c,
  0x0580d73a2d880f4f, 0x0b01ae745b101e9e, 0x16035ce8b6203d3d, 0x04671294f139a5d9,
  0x08ce2529e2734bb2, 0x119c4a53c4e69764, 0x0385a8772761517a, 0x070b50ee4ec2a2f5,
  0x0e16a1dc9d8545e9, 0x02d1539285e77462, 0x05a2a7250bcee8c4, 0x0b454e4a179dd187,
  0x168a9c942f3ba30f, 0x04821f50d63f209d, 0x09043ea1ac7e4139, 0x12087d4358fc8272,
  0x039b4c40ab65b3b0, 0x0736988156cb6761, 0x0e6d3102ad96cec2, 0x02e2a366ef848fc0,
  0x05c546cddf091f81, 0x0b8a8d9bbe123f01, 0x17151b377c247e03, 0x049dd23e4c074c67,
  0x093ba47c980e98ce, 0x127748f9301d319c, 0x03b174fea33909ec, 0x0762e9fd467213d8,
  0x0ec5d3fa8ce427b0, 0x02f45d98829407f0, 0x05e8bb3105280fe0, 0x0bd176620a501fc0,
  0x17a2ecc414a03f80, 0x04ba2f5a6a86731a, 0x09745eb4d50ce633, 0x12e8bd69aa19cc66,
  0x03c825e1eed1f5ae, 0x07904bc3dda3eb5c, 0x0f209787bb47d6b8, 0x030684b4bf0e5e25,
  0x060d09697e1cbc4a, 0x0c1a12d2fc397893, 0x183425a5f872f127, 0x04d73abacb4a303b,
  0x09ae757596946076, 0x135ceaeb2d28c0ec, 0x03df622f09082696, 0x07bec45e12104d2b,
  0x0f7d88bc24209a56, 0x03191b58d4068544, 0x063236b1a80d0a89, 0x0c646d63501a1512,
  0x18c8dac6a0342a24, 0x04f4f88e200a6ed4, 0x09e9f11c4014dda8, 0x13d3e2388029bb50,
  0x03f72d3e800858aa, 0x07ee5a7d0010b153, 0x0fdcb4fa002162a6, 0x032c24320006ad54,
  0x06584864000d5aa9, 0x0cb090c8001ab552, 0x1961219000356aa4, 0x05136d1cccd77bba,
  0x0a26da3999aef775, 0x144db473335deee9, 0x040f8a7d70ac62fb, 0x081f14fae158c5f7,
  0x103e29f5c2b18bee, 0x033fa1fdf3bd1bfc, 0x067f43fbe77a37f9, 0x0cfe87f7cef46ff1,
  0x19fd0fef9de8dfe3, 0x05329cc985fb5ffa, 0x0a6539930bf6bff4, 0x14ca732617ed7fe9,
  0x04287d6e04c91995, 0x0850fadc0992332a, 0x10a1f5b813246654, 0x0353978b370747aa,
  0x06a72f166e0e8f55, 0x0d4e5e2cdc1d1ea9, 0x1a9cbc59b83a3d53, 0x05528c11f1a53f77,
  0x0aa51823e34a7eee, 0x154a3047c694fddc, 0x044209a7f48432c6, 0x0884134fe908658b,
  0x1108269fd210cb16, 0x036807b99069c238, 0x06d00f7320d3846f, 0x0da01ee641a708df,
  0x1b403dcc834e11bd, 0x05733f8f4d76038c, 0x0ae67f1e9aec0718, 0x15ccfe3d35d80e31,
  0x045c32d90ac4cfa3, 0x08b865b215899f47, 0x1170cb642b133e8e, 0x037cf57a6f03d950,
  0x06f9eaf4de07b29f, 0x0df3d5e9bc0f653e, 0x02ca5dfb8c031440, 0x0594bbf71806287f,
  0x0b2977ee300c50fe, 0x1652efdc6018a1fd, 0x0476fcc5acd1b9ff, 0x08edf98b59a373ff,
  0x11dbf316b346e7fe, 0x039263d1570e2e66, 0x0724c7a2ae1c5ccc, 0x0e498f455c38b998,
  0x02db830ddf3e8b85, 0x05b7061bbe7d1709, 0x0b6e0c377cfa2e13, 0x16dc186ef9f45c26,
  0x04926b496530df3b, 0x0924d692ca61be76, 0x1249ad2594c37ceb, 0x03a855d450f3e5c9,
  0x0750aba8a1e7cb91, 0x0ea1575143cf9722, 0x02ed1176a72984a0, 0x05da22ed4e530941,
  0x0bb445da9ca61282, 0x17688bb5394c2504, 0x04ae825771dc0767, 0x095d04aee3b80ece,
  0x12ba095dc7701d9d, 0x03beceac5b166c53, 0x077d9d58b62cd8a5, 0x0efb3ab16c59b14a,
  0x02ff0bbd15ab89dc, 0x05fe177a2b5713b7, 0x0bfc2ef456ae276f, 0x17f85de8ad5c4edd,
  0x04cb45fb55df42f9, 0x09968bf6abbe85f2, 0x132d17ed577d0be4, 0x03d5d195de4c3594,
  0x07aba32bbc986b28, 0x0f5746577930d650, 0x03117477e509c476, 0x0622e8efca1388ed,
  0x0c45d1df942711da, 0x188ba3bf284e23b3, 0x04e8ba596e760724, 0x09d174b2dcec0e48,
  0x13a2e965b9d81c8f, 0x03ed61e1252b38e9, 0x07dac3c24a5671d3, 0x0fb5878494ace3a6,
  0x03244e4db755c721, 0x06489c9b6eab8e42, 0x0c913936dd571c85, 0x1922726dbaae390a,
  0x0506e3af8bbc71cf, 0x0a0dc75f1778e39d, 0x141b8ebe2ef1c73b, 0x040582f2d6305b0c,
  0x080b05e5ac60b618, 0x10160bcb58c16c2f, 0x03379bf57826af3d, 0x066f37eaf04d5e79,
  0x0cde6fd5e09abcf2, 0x19bcdfabc13579e5, 0x0525c6558d0ab1fb, 0x0a4b8cab1a1563f5,
  0x14971956342ac7ea, 0x041e384470d55b2f, 0x083c7088e1aab65e, 0x1078e111c3556cbb,
  0x034b6036c0aaaf59, 0x0696c06d81555eb1, 0x0d2d80db02aabd63, 0x1a5b01b605557ac5,
  0x054566be0111188e, 0x0a8acd7c0222311c, 0x15159af804446238, 0x04378564cda746d8,
  0x086f0ac99b4e8db0, 0x10de1593369d1b60, 0x035f9dea3e1f6be0, 0x06bf3bd47c3ed7c0,
  0x0d7e77a8f87daf80, 0x1afcef51f0fb5eff, 0x0565c976c9cbdfcd, 0x0acb92ed9397bf99,
  0x159725db272f7f33, 0x04516df8a16fe63d, 0x08a2dbf142dfcc7b, 0x1145b7e285bf98f5,
  0x037457fa1abfeb64, 0x06e8aff4357fd6c9, 0x0dd15fe86affad91, 0x1ba2bfd0d5ff5b22,
  0x0586f329c466456d, 0x0b0de65388cc8adb, 0x161bcca7119915b5, 0x046bf5bb03850457,
  0x08d7eb76070a08af, 0x11afd6ec0e14115e, 0x03899162693736ac, 0x071322c4d26e6d59,
  0x0e264589a4dcdab1, 0x02d4744eba929223, 0x05a8e89d75252447, 0x0b51d13aea4a488e,
  0x16a3a275d494911c, 0x0487207df750e9d2, 0x090e40fbeea1d3a5, 0x121c81f7dd43a749,
  0x039f4d3192a72175, 0x073e9a63254e42ea, 0x0e7d34c64a9c85d4, 0x02e5d75adbb8e791,
  0x05cbaeb5b771cf22, 0x0b975d6b6ee39e43, 0x172ebad6ddc73c87, 0x04a2f22af927d8e8,
  0x0945e455f24fb1d0, 0x128bc8abe49f639f, 0x03b58e88c75313ed, 0x076b1d118ea627d9,
  0x0ed63a231d4c4fb2, 0x02f7a53a390f4324, 0x05ef4a74721e8647, 0x0bde94e8e43d0c8f,
  0x17bd29d1c87a191e, 0x04bf6ec38e7ed1d3, 0x097edd871cfda3a5, 0x12fdbb0e39fb474b,
  0x03cc589c71ff0e42, 0x0798b138e3fe1c84, 0x0f316271c7fc3909, 0x0309e07d27ff3e9b,
  0x0613c0fa4ffe7d37, 0x0c2781f49ffcfa6d, 0x184f03e93ff9f4db, 0x04dc9a61d998642c,
  0x09b934c3b330c857, 0x13726987666190af, 0x03e3aeb4ae138356, 0x07c75d695c2706ac,
  0x0f8ebad2b84e0d59, 0x031c8bc3be7602ab, 0x063917877cec0557, 0x0c722f0ef9d80aad,
  0x18e45e1df3b0155b, 0x04fa793930bcd112, 0x09f4f2726179a224, 0x13e9e4e4c2f34449,
  0x03fb942dc0970da8, 0x07f7285b812e1b50, 0x0fee50b7025c36a1, 0x032fa9be33ac0aed,
  0x065f537c675815da, 0x0cbea6f8ceb02bb4, 0x197d4df19d605767, 0x05190f96b91344ae,
  0x0a321f2d7226895c, 0x14643e5ae44d12b9, 0x04140c78940f6a25, 0x082818f1281ed44a,
  0x105031e2503da894, 0x03433d2d433f881e, 0x06867a5a867f103b, 0x0d0cf4b50cfe2076,
  0x1a19e96a19fc40ed, 0x053861e205327363, 0x0a70c3c40a64e6c5, 0x14e1878814c9cd8a,
  0x042d1b1b375b8f82, 0x085a36366eb71f04, 0x10b46c6cdd6e3e08, 0x035748e292afa602,
  0x06ae91c5255f4c03, 0x0d5d238a4abe9807, 0x1aba4714957d300d, 0x0558749db77f7003,
  0x0ab0e93b6efee005, 0x1561d276ddfdc00a, 0x0446c3b15f992668, 0x088d8762bf324cd1,
  0x111b0ec57e6499a2, 0x036bcfc1194751ed, 0x06d79f82328ea3da, 0x0daf3f04651d47b5,
  0x1b5e7e08ca3a8f6a, 0x05794c6828721caf, 0x0af298d050e4395d, 0x15e531a0a1c872bb,
  0x046109eced2816f2, 0x08c213d9da502de4, 0x118427b3b4a05bc9, 0x0380d4bd8a8678c2,
  0x0701a97b150cf183, 0x0e0352f62a19e307, 0x02cd76fe086b93ce, 0x059aedfc10d7279c,
  0x0b35dbf821ae4f39, 0x166bb7f0435c9e71, 0x047bf19673df52e3, 0x08f7e32ce7bea5c7,
  0x11efc659cf7d4b8e, 0x03965adec3190f1c, 0x072cb5bd86321e39, 0x0e596b7b0c643c72,
  0x02deaf189c140c17, 0x05bd5e313828182d, 0x0b7abc627050305b, 0x16f578c4e0a060b6,
  0x04977e8dc68679be, 0x092efd1b8d0cf37c, 0x125dfa371a19e6f8, 0x03ac653e386b9498,
  0x0758ca7c70d72930, 0x0eb194f8e1ae5260, 0x02f050fe938943ad, 0x05e0a1fd2712875a,
  0x0bc143fa4e250eb3, 0x178287f49c4a1d66, 0x04b3b4ca85a86c48, 0x096769950b50d88f,
  0x12ced32a16a1b11f, 0x03c2f7086aed236d, 0x0785ee10d5da46d9, 0x0f0bdc21abb48db2,
  0x03025f39ef241c57, 0x0604be73de4838ae, 0x0c097ce7bc90715b, 0x1812f9cf7920e2b6,
  0x04d0985cb1d3608b, 0x09a130b963a6c116, 0x13426172c74d822c, 0x03da137d5b0f806f,
  0x07b426fab61f00de, 0x0f684df56c3e01bc, 0x0314dc6448d9338c, 0x0629b8c891b26718,
  0x0c5371912364ce30, 0x18a6e32246c99c61, 0x04ee2d6d415b85ad, 0x09dc5ada82b70b5a,
  0x13b8b5b5056e16b4, 0x03f1bdf10116048a, 0x07e37be2022c0915, 0x0fc6f7c404581229,
  0x0327cb2734119d3b, 0x064f964e68233a77, 0x0c9f2c9cd04674ee, 0x193e5939a08ce9dc,
  0x050c783eb9b5c85f, 0x0a18f07d736b90be, 0x1431e0fae6d7217d, 0x0409f9cbc7c4a04c,
  0x0813f3978f894098, 0x1027e72f1f128131, 0x033b2e3c9fd0803d, 0x06765c793fa1007a,
  0x0cecb8f27f4200f4, 0x19d971e4fe8401e7, 0x052b7d2dcc80cd2e, 0x0a56fa5b99019a5c,
  0x14adf4b7320334b9, 0x0422ca8b0a00a425, 0x084595161401484a, 0x108b2a2c28029094,
  0x034f086f3b33b684, 0x069e10de76676d08, 0x0d3c21bcecceda10, 0x1a784379d99db420,
  0x054b40b1f852bda0, 0x0a968163f0a57b40, 0x152d02c7e14af680, 0x043c33c193756480,
  0x0878678326eac900, 0x10f0cf064dd59200, 0x03635c9adc5dea00, 0x06c6b935b8bbd400,
  0x0d8d726b7177a800, 0x1b1ae4d6e2ef5000, 0x056bc75e2d631000, 0x0ad78ebc5ac62000,
  0x15af1d78b58c4000, 0x04563918244f4000, 0x08ac7230489e8000, 0x1158e460913d0000,
  0x03782dace9d90000, 0x06f05b59d3b20000, 0x0de0b6b3a7640000, 0x1bc16d674ec80000,
  0x058d15e176280000, 0x0b1a2bc2ec500000, 0x16345785d8a00000, 0x0470de4df8200000,
  0x08e1bc9bf0400000, 0x11c37937e0800000, 0x038d7ea4c6800000, 0x071afd498d000000,
  0x0e35fa931a000000, 0x02d79883d2000000, 0x05af3107a4000000, 0x0b5e620f48000000,
  0x16bcc41e90000000, 0x048c273950000000, 0x09184e72a0000000, 0x12309ce540000000,
  0x03a3529440000000, 0x0746a52880000000, 0x0e8d4a5100000000, 0x02e90edd00000000,
  0x05d21dba00000000, 0x0ba43b7400000000, 0x174876e800000000, 0x04a817c800000000,
  0x09502f9000000000, 0x12a05f2000000000, 0x03b9aca000000000, 0x0773594000000000,
  0x0ee6b28000000000, 0x02faf08000000000, 0x05f5e10000000000, 0x0bebc20000000000,
  0x17d7840000000000, 0x04c4b40000000000, 0x0989680000000000, 0x1312d00000000000,
  0x03d0900000000000, 0x07a1200000000000, 0x0f42400000000000, 0x030d400000000000,
  0x061a800000000000, 0x0c35000000000000, 0x186a000000000000, 0x04e2000000000000,
  0x09c4000000000000, 0x1388000000000000, 0x03e8000000000000, 0x07d0000000000000,
  0x0fa0000000000000, 0x0320000000000000, 0x0640000000000000, 0x0c80000000000000,
  0x1900000000000000, 0x0500000000000000, 0x0a00000000000000, 0x1400000000000000,
  0x0400000000000000, 0x0800000000000000, 0x1000000000000000, 0x0333333333333333,
  0x0666666666666666, 0x0ccccccccccccccd, 0x199999999999999a, 0x051eb851eb851eb8,
  0x0a3d70a3d70a3d71, 0x147ae147ae147ae1, 0x04189374bc6a7efa, 0x083126e978d4fdf4,
  0x10624dd2f1a9fbe7, 0x0346dc5d63886595, 0x068db8bac710cb29, 0x0d1b71758e219653,
  0x1a36e2eb1c432ca5, 0x053e2d6238da3c21, 0x0a7c5ac471b47842, 0x14f8b588e368f084,
  0x0431bde82d7b634e, 0x08637bd05af6c69b, 0x10c6f7a0b5ed8d37, 0x035afe535795e90b,
  0x06b5fca6af2bd216, 0x0d6bf94d5e57a42c, 0x1ad7f29abcaf4858, 0x055e63b88c230e78,
  0x0abcc77118461cf0, 0x15798ee2308c39e0, 0x044b82fa09b5a52d, 0x089705f4136b4a59,
  0x112e0be826d694b3, 0x036f9bfb3af7b757, 0x06df37f675ef6eae, 0x0dbe6fecebdedd5c,
  0x1b7cdfd9d7bdbab8, 0x057f5ff85e592558, 0x0afebff0bcb24ab0, 0x15fd7fe179649560,
  0x0465e6604b7a8446, 0x08cbccc096f5088d, 0x119799812dea1119, 0x0384b84d092ed038,
  0x0709709a125da071, 0x0e12e13424bb40e1, 0x02d09370d4257360, 0x05a126e1a84ae6c0,
  0x0b424dc35095cd81, 0x16849b86a12b9b02, 0x0480ebe7b9d58567, 0x0901d7cf73ab0ace,
  0x1203af9ee756159b, 0x039a5652fb113785, 0x0734aca5f6226f0b, 0x0e69594bec44de16,
  0x02e1dea8c8da92d1, 0x05c3bd5191b525a2, 0x0b877aa3236a4b45, 0x170ef54646d49689,
  0x049c97747490eae8, 0x09392ee8e921d5d0, 0x12725dd1d243aba1, 0x03b07929f6da5587,
  0x0760f253edb4ab0d, 0x0ec1e4a7db69561a, 0x02f394219248446c, 0x05e72843249088d7,
  0x0bce5086492111af, 0x179ca10c9242235d, 0x04b8ed0283a6d3df, 0x0971da05074da7bf,
  0x12e3b40a0e9b4f7e, 0x03c7240202ebdcb3, 0x078e480405d7b966, 0x0f1c90080baf72cb,
  0x0305b66802564a29, 0x060b6cd004ac9451, 0x0c16d9a0095928a2, 0x182db34012b25145,
  0x04d5f0a66a23a9db, 0x09abe14cd44753b5, 0x1357c299a88ea76a, 0x03de5a1ebb4fbb15,
  0x07bcb43d769f762b, 0x0f79687aed3eec55, 0x0318481895d96277, 0x063090312bb2c4ef,
  0x0c612062576589de, 0x18c240c4aecb13bb, 0x04f3a68dbc8f03f2, 0x09e74d1b791e07e5,
  0x13ce9a36f23c0fc9, 0x03f61ed7ca0c0328, 0x07ec3daf94180650, 0x0fd87b5f28300ca1,
  0x032b4bdfd4d668ed, 0x065697bfa9acd1da, 0x0cad2f7f5359a3b4, 0x195a5efea6b34768,
  0x051212ffbaf0a7e2, 0x0a2425ff75e14fc3, 0x14484bfeebc29f86, 0x040e7599625a1fe8,
  0x081ceb32c4b43fcf, 0x1039d66589687f9f, 0x033ec47ab514e653, 0x067d88f56a29cca6,
  0x0cfb11ead453994c, 0x19f623d5a8a73297, 0x05313a5dee87d6eb, 0x0a6274bbdd0fadd6,
  0x14c4e977ba1f5bac, 0x042761e4bed31256, 0x084ec3c97da624ab, 0x109d8792fb4c4957,
  0x0352b4b6ff0f41de, 0x06a5696dfe1e83bc, 0x0d4ad2dbfc3d0778, 0x1a95a5b7f87a0ef1,
  0x05512124cb4b9c97, 0x0aa242499697392d, 0x154484932d2e725a, 0x0440e750a2a2e3ac,
  0x0881cea14545c757, 0x11039d428a8b8eaf, 0x03671f73b54f1c89, 0x06ce3ee76a9e3913,
  0x0d9c7dced53c7225, 0x1b38fb9daa78e44b, 0x0571cbec554b60dc, 0x0ae397d8aa96c1b7,
  0x15c72fb1552d836f, 0x045b0989ddd5e716, 0x08b61313bbabce2c, 0x116c262777579c59,
  0x037c07a17e44b8df, 0x06f80f42fc8971bd, 0x0df01e85f912e37a, 0x1be03d0bf225c6f4,
  0x05933f68ca078e31, 0x0b267ed1940f1c62, 0x164cfda3281e38c4, 0x0475cc53d4d2d827,
  0x08eb98a7a9a5b04e, 0x11d7314f534b609c, 0x0391704310a8acec, 0x0722e086215159d8,
  0x0e45c10c42a2b3b0, 0x02dac035a6ed5723, 0x05b5806b4ddaae47, 0x0b6b00d69bb55c8d,
  0x16d601ad376ab91a, 0x049133890b155838, 0x09226712162ab071, 0x1244ce242c5560e2,
  0x03a75c6da27779c7, 0x074eb8db44eef38d, 0x0e9d71b689dde71b, 0x02ec49f14ec5fb05,
  0x05d893e29d8bf60b, 0x0bb127c53b17ec16, 0x17624f8a762fd82b, 0x04ad431bb13cc4d5,
  0x095a8637627989ab, 0x12b50c6ec4f31356, 0x03bdcf495a9703de, 0x077b9e92b52e07bc,
  0x0ef73d256a5c0f78, 0x02fe3f6de212697e, 0x05fc7edbc424d2fd, 0x0bf8fdb78849a5f9,
  0x17f1fb6f10934bf3, 0x04c9ff163683dbfd, 0x0993fe2c6d07b7fb, 0x1327fc58da0f6ff5,
  0x03d4cc11c5364997, 0x07a998238a6c932f, 0x0f53304714d9265e, 0x0310a3416a91d479,
  0x06214682d523a8f2, 0x0c428d05aa4751e5, 0x18851a0b548ea3ca, 0x04e76b9bddb620c2,
  0x09ced737bb6c4184, 0x139dae6f76d88308, 0x03ec56164af81a35, 0x07d8ac2c95f03469,
  0x0fb158592be068d3, 0x03237811d593482a, 0x0646f023ab269054, 0x0c8de047564d20a9,
  0x191bc08eac9a4151, 0x05058ce955b87377, 0x0a0b19d2ab70e6ed, 0x141633a556e1cddb,
  0x040470baaaf9f5f9, 0x0808e17555f3ebf1, 0x1011c2eaabe7d7e2, 0x0336c0955594c4c7,
  0x066d812aab29898e, 0x0cdb02555653131b, 0x19b604aaaca62637, 0x0524675555bad471,
  0x0a48ceaaab75a8e3, 0x14919d5556eb51c5, 0x041d1f7777c8a9f4, 0x083a3eeeef9153e9,
  0x10747ddddf22a7d1, 0x034a7f92c63a2190, 0x0694ff258c744320, 0x0d29fe4b18e88641,
  0x1a53fc9631d10c82, 0x0543ff513d29cf4d, 0x0a87fea27a539e9a, 0x150ffd44f4a73d35,
  0x043665da9754a5d7, 0x086ccbb52ea94baf, 0x10d9976a5d52975d, 0x035eb7e212aa1e46,
  0x06bd6fc425543c8c, 0x0d7adf884aa87917, 0x1af5bf109550f22f, 0x05645969b77696d6,
  0x0ac8b2d36eed2dac, 0x159165a6ddda5b59, 0x04504787c5f878ab, 0x08a08f0f8bf0f157,
  0x11411e1f17e1e2ad, 0x03736c6c9e606089, 0x06e6d8d93cc0c112, 0x0dcdb1b279818224,
  0x1b9b6364f3030449, 0x05857a4763cd6742, 0x0b0af48ec79ace83, 0x1615e91d8f359d07,
  0x046ac8391ca4529b, 0x08d590723948a536, 0x11ab20e472914a6c, 0x0388a02db0837549,
  0x0711405b6106ea92, 0x0e2280b6c20dd523, 0x02d3b357c0692aa1, 0x05a766af80d25541,
  0x0b4ecd5f01a4aa83, 0x169d9abe03495505, 0x0485ebbf9a41ddce, 0x090bd77f3483bb9c,
  0x1217aefe69077737, 0x039e5632e1ce4b0b, 0x073cac65c39c9616, 0x0e7958cb87392c2c,
  0x02e511c24e3ea26f, 0x05ca23849c7d44de, 0x0b94470938fa89bd, 0x17288e1271f5137a,
  0x04a1b603b0643718, 0x09436c0760c86e31, 0x1286d80ec190dc61, 0x03b4919c8d1cf8e0,
  0x076923391a39f1c1, 0x0ed246723473e381, 0x02f6dae3a4172d80, 0x05edb5c7482e5b00,
  0x0bdb6b8e905cb601, 0x17b6d71d20b96c02, 0x04be2b05d35848cd, 0x097c560ba6b0919a,
  0x12f8ac174d612335, 0x03cb559e42ad070b, 0x0796ab3c855a0e15, 0x0f2d56790ab41c2a,
  0x0309114b688a6c08, 0x06122296d114d811, 0x0c24452da229b022, 0x18488a5b44536043,
  0x04db4edf0daa4674, 0x09b69dbe1b548ce8, 0x136d3b7c36a919d0, 0x03e2a57f3e21d1f6,
  0x07c54afe7c43a3ed, 0x0f8a95fcf88747d9, 0x031bb798fe8174c5, 0x06376f31fd02e98a,
  0x0c6ede63fa05d314, 0x18ddbcc7f40ba628, 0x04f925c1973587a2, 0x09f24b832e6b0f43,
  0x13e497065cd61e87, 0x03fa849adf5e061b, 0x07f50935bebc0c36, 0x0fea126b7d78186c,
  0x032ed07be5e4d1af, 0x065da0f7cbc9a35e, 0x0cbb41ef979346bd, 0x197683df2f268d79,
  0x0517b3f96fd482b2, 0x0a2f67f2dfa90564, 0x145ecfe5bf520ac7, 0x0412f66126439bc1,
  0x0825ecc24c873783, 0x104bd984990e6f06, 0x03425eb41e9c7c9b, 0x0684bd683d38f936,
  0x0d097ad07a71f26b, 0x1a12f5a0f4e3e4d6, 0x0536fdecfdc72dc4, 0x0a6dfbd9fb8e5b89,
  0x14dbf7b3f71cb712, 0x042bfe57316c249d, 0x0857fcae62d8493a, 0x10aff95cc5b09275,
  0x035665128df01d4b, 0x06acca251be03a95, 0x0d59944a37c0752a, 0x1ab328946f80ea54,
  0x0557081dafe69544, 0x0aae103b5fcd2a88, 0x155c2076bf9a5510, 0x0445a017bfebaa9d,
  0x088b402f7fd7553a, 0x1116805effaeaa73, 0x036ae67966562217, 0x06d5ccf2ccac442e,
  0x0dab99e59958885c, 0x1b5733cb32b110b9, 0x0577d728a3bd0358, 0x0aefae51477a06b0,
  0x15df5ca28ef40d60, 0x045fdf53b630cf7a, 0x08bfbea76c619ef3, 0x117f7d4ed8c33de7,
  0x037fe5dc91c0a5fb, 0x06ffcbb923814bf6, 0x0dff9772470297ec, 0x02ccb7e3a7cd5196,
  0x05996fc74f9aa32b, 0x0b32df8e9f354656, 0x1665bf1d3e6a8cad, 0x047abfd2a6154f56,
  0x08f57fa54c2a9eab, 0x11eaff4a98553d57, 0x039566421e7772ab, 0x072acc843ceee556,
  0x0e55990879ddcaac, 0x02ddeb68185f8eef, 0x05bbd6d030bf1dde, 0x0b77ada0617e3bbd,
  0x16ef5b40c2fc7779, 0x049645735a327e4b, 0x092c8ae6b464fc97, 0x125915cd68c9f92e,
  0x03ab6ac2ae8ecb70, 0x0756d5855d1d96df, 0x0eadab0aba3b2dbe, 0x02ef889bbed8a2c0,
  0x05df11377db1457f, 0x0bbe226efb628aff, 0x177c44ddf6c515fd, 0x04b2742c648dd133,
  0x0964e858c91ba265, 0x12c9d0b1923744cb, 0x03c1f689ea0b0dc2, 0x0783ed13d4161b84,
  0x0f07da27a82c3709, 0x03019207ee6f3e35, 0x0603240fdcde7c6a, 0x0c06481fb9bcf8d4,
  0x180c903f7379f1a7, 0x04cf500cb0b1fd21, 0x099ea0196163fa43, 0x133d4032c2c7f486,
  0x03d90cd6f3c1974e, 0x07b219ade7832e9c, 0x0f64335bcf065d38, 0x03140a458fce12a5,
  0x0628148b1f9c254a, 0x0c5029163f384a93, 0x18a0522c7e709526, 0x04ecdd3c1949b76e,
  0x09d9ba7832936edc, 0x13b374f06526ddb8, 0x03f0b0fce107c5f2, 0x07e161f9c20f8be3,
  0x0fc2c3f3841f17c6, 0x0326f3fd80d304c1, 0x064de7fb01a60983, 0x0c9bcff6034c1305,
  0x19379fec0698260a, 0x050b1ffc0151a135, 0x0a163ff802a3426b, 0x142c7ff0054684d5,
  0x0408e66334414dc4, 0x0811ccc668829b88, 0x1023998cd1053711, 0x033a51e8f69aa49d,
  0x0674a3d1ed35493a, 0x0ce947a3da6a9274, 0x19d28f47b4d524e8, 0x052a1ca7f0f76dc8,
  0x0a54394fe1eedb90, 0x14a8729fc3ddb720, 0x0421b0865a5f8b06, 0x0843610cb4bf160d,
  0x1086c219697e2c19, 0x034e26d1e1e608d2, 0x069c4da3c3cc11a4, 0x0d389b4787982348,
  0x1a71368f0f30468f, 0x0549d7b6363cdae9, 0x0a93af6c6c79b5d3, 0x15275ed8d8f36ba6,
  0x043b12f82b63e254, 0x087625f056c7c4a9, 0x10ec4be0ad8f8951, 0x0362759355e981dd,
  0x06c4eb26abd303ba, 0x0d89d64d57a60774, 0x1b13ac9aaf4c0ee9, 0x056a55b889759c95,
  0x0ad4ab7112eb392a, 0x15a956e225d67254, 0x045511606df7b077, 0x08aa22c0dbef60ee,
  0x11544581b7dec1dd, 0x03774119f192f393, 0x06ee8233e325e725, 0x0ddd0467c64bce4a,
  0x1bba08cf8c979c94, 0x058b9b5cb5b7ec1e, 0x0b1736b96b6fd83b, 0x162e6d72d6dfb076,
  0x046faf7d5e2cbce4, 0x08df5efabc5979c9, 0x11bebdf578b2f392, 0x038c8c644b56fd84,
  0x071918c896adfb07, 0x0e3231912d5bf60e, 0x02d6d6b6a2abfe03, 0x05adad6d4557fc06,
  0x0b5b5ada8aaff80c, 0x16b6b5b5155ff017, 0x048af1243779966b, 0x0915e2486ef32cd6,
  0x122bc490dde659ac, 0x03a25a835f947856, 0x0744b506bf28f0ab, 0x0e896a0d7e51e156,
  0x02e8486919439378, 0x05d090d2328726ef, 0x0ba121a4650e4ddf, 0x17424348ca1c9bbd,
  0x04a6da41c205b8bf, 0x094db483840b717f, 0x129b69070816e2fe, 0x03b8ae9b019e2d66,
  0x07715d36033c5acc, 0x0ee2ba6c0678b598, 0x02fa2548ce182452, 0x05f44a919c3048a3,
  0x0be8952338609146, 0x17d12a4670c1228d, 0x04c36edae359d3b6, 0x0986ddb5c6b3a76b,
  0x130dbb6b8d674ed7, 0x03cf8be24f7b0fc5, 0x079f17c49ef61f89, 0x0f3e2f893dec3f12,
  0x030c6fe83f95a637, 0x0618dfd07f2b4c6e, 0x0c31bfa0fe5698dc, 0x18637f41fcad31b7,
  0x04e0b30d328909f1, 0x09c1661a651213e3, 0x1382cc34ca2427c6, 0x03e6f5a4286da18e,
  0x07cdeb4850db431c, 0x0f9bd690a1b68638, 0x031f2ae9b9f14e0b, 0x063e55d373e29c16,
  0x0c7caba6e7c5382d, 0x18f9574dcf8a7059, 0x04feab0f8fe87cdf, 0x09fd561f1fd0f9bd,
  0x13faac3e3fa1f37a, 0x03feef3fa6539718, 0x07fdde7f4ca72e31, 0x0ffbbcfe994e5c62,
  0x033258ffb842df47, 0x0664b1ff7085be8e, 0x0cc963fee10b7d1b, 0x1992c7fdc216fa36,
  0x051d5b32c06afed8, 0x0a3ab66580d5fdaf, 0x14756ccb01abfb5f, 0x04177c2899ef3246,
  0x082ef85133de648c, 0x105df0a267bcc919, 0x0345fced47f28e9f, 0x068bf9da8fe51d3d,
  0x0d17f3b51fca3a7a, 0x1a2fe76a3f9474f4, 0x053cc7e20cb74a97, 0x0a798fc4196e952e,
  0x14f31f8832dd2a5d, 0x04309fe80a2c3bac, 0x08613fd014587758, 0x10c27fa028b0eeb1,
  0x035a19866e89c957, 0x06b4330cdd1392ad, 0x0d686619ba27255a, 0x1ad0cc33744e4ab4,
  0x055cf5a3e40fa88a, 0x0ab9eb47c81f5115, 0x1573d68f903ea22a, 0x044a5e1cb672ed3c,
  0x0894bc396ce5da77, 0x11297872d9cbb4ee, 0x036eb1b091f58a96, 0x06dd636123eb152c,
  0x0dbac6c247d62a58, 0x1b758d848fac54b0, 0x057de91a83227756, 0x0afbd2350644eead,
  0x15f7a46a0c89dd5a, 0x0464ba7b9c1b92ac, 0x08c974f738372557, 0x1192e9ee706e4aae,
  0x0383c862e3494223, 0x070790c5c6928446, 0x0e0f218b8d25088c, 0x02cfd3824f6dce82,
  0x059fa7049edb9d05, 0x0b3f4e093db73a09, 0x167e9c127b6e7412, 0x047fb8d07f161737,
  0x08ff71a0fe2c2e6e, 0x11fee341fc585cdc, 0x039960a6cc11ac2c, 0x0732c14d98235858,
  0x0e65829b3046b0b0, 0x02e11a1f09a7bcf0, 0x05c2343e134f79e0, 0x0b84687c269ef3c0,
  0x1708d0f84d3de77f, 0x049b5cfe75d92e4d, 0x0936b9fcebb25c99, 0x126d73f9d764b933,
  0x03af7d985e47583d, 0x075efb30bc8eb07b, 0x0ebdf661791d60f5, 0x02f2cae04b6c4697,
  0x05e595c096d88d2f, 0x0bcb2b812db11a5e, 0x179657025b6234bc, 0x04b7ab0078ad3dbf,
  0x096f5600f15a7b7e, 0x12deac01e2b4f6fd, 0x03c62266c6f0fe33, 0x078c44cd8de1fc65,
  0x0f18899b1bc3f8ca, 0x0304e85238c0cb5c, 0x0609d0a4718196b7, 0x0c13a148e3032d6e,
  0x18274291c6065add, 0x04d4a6e9f467abc6, 0x09a94dd3e8cf578c, 0x13529ba7d19eaf17,
  0x03dd5254c3862305, 0x07baa4a9870c4609, 0x0f7549530e188c13, 0x031775109c6b4f37,
  0x062eea2138d69e6e, 0x0c5dd44271ad3cdc, 0x18bba884e35a79b7, 0x04f254e760abb1f1,
  0x09e4a9cec15763e3, 0x13c9539d82aec7c6, 0x03f510b91a22f4c1, 0x07ea21723445e982,
  0x0fd442e4688bd305, 0x032a73c7481bf701, 0x0654e78e9037ee02, 0x0ca9cf1d206fdc04,
  0x19539e3a40dfb807, 0x0510b93ed9c65801, 0x0a21727db38cb003, 0x1442e4fb67196006,
  0x040d60ff149eacce, 0x081ac1fe293d599c, 0x103583fc527ab338, 0x033de73276e5570b,
  0x067bce64edcaae16, 0x0cf79cc9db955c2d, 0x19ef3993b72ab85a, 0x052fd850be3bbe78,
  0x0a5fb0a17c777cf1, 0x14bf6142f8eef9e1, 0x042646a6fe9631fa, 0x084c8d4dfd2c63f4,
  0x10991a9bfa58c7e7, 0x0351d21f3211c195, 0x06a3a43e64238329, 0x0d47487cc8470653,
  0x1a8e90f9908e0ca5, 0x054fb698501c68ee, 0x0a9f6d30a038d1dc, 0x153eda614071a3b8,
  0x043fc546a67d20be, 0x087f8a8d4cfa417d, 0x10ff151a99f482f9, 0x0366376bb8641a32,
  0x06cc6ed770c83464, 0x0d98ddaee19068c7, 0x1b31bb5dc320d18f, 0x057058ac5a39c383,
  0x0ae0b158b4738706, 0x15c162b168e70e0c, 0x0459e089e1c7cf9c, 0x08b3c113c38f9f38,
  0x11678227871f3e70, 0x037b1a07e7d30c7d, 0x06f6340fcfa618fa, 0x0dec681f9f4c31f3,
  0x1bd8d03f3e9863e6, 0x0591c33fd951ad94, 0x0b23867fb2a35b29, 0x16470cff6546b652,
  0x04749c33144157aa, 0x08e938662882af54, 0x11d270cc51055ea8, 0x03907cf5a9cddfbb,
  0x0720f9eb539bbf76, 0x0e41f3d6a7377eed, 0x02d9fd9154a4b2fc, 0x05b3fb22a94965f8,
  0x0b67f6455292cbf1, 0x16cfec8aa52597e1, 0x048ffc1bbaa11e60, 0x091ff83775423cc0,
  0x123ff06eea847981, 0x03a66349621a7eb3, 0x074cc692c434fd67, 0x0e998d258869facd,
  0x02eb82a11b48655c, 0x05d705423690cab9, 0x0bae0a846d219571, 0x175c1508da432ae2,
  0x04ac0434f873d560, 0x09580869f0e7aac1, 0x12b010d3e1cf5582, 0x03bcd02a605caab4,
  0x0779a054c0b95567, 0x0ef340a98172aace, 0x02fd735519e3bbc3, 0x05fae6aa33c77786,
  0x0bf5cd54678eef0b, 0x17eb9aa8cf1dde17, 0x04c8b888296c5f9e, 0x0991711052d8bf3c,
  0x1322e220a5b17e79, 0x03d3c6d35456b2e5, 0x07a78da6a8ad65ca, 0x0f4f1b4d515acb94,
  0x030fd242a9def584, 0x061fa48553bdeb08, 0x0c3f490aa77bd610, 0x187e92154ef7ac20,
  0x04e61d37763188d3, 0x09cc3a6eec6311a6, 0x139874ddd8c6234c, 0x03eb4a92c4f46d76,
  0x07d6952589e8daeb, 0x0fad2a4b13d1b5d7, 0x0322a20f03f6bdf8, 0x0645441e07ed7bf0,
  0x0c8a883c0fdaf7df, 0x191510781fb5efbe, 0x0504367e6cbdfcc0, 0x0a086cfcd97bf97f,
  0x1410d9f9b2f7f2fe, 0x04035ecb8a319700, 0x0806bd9714632dff, 0x100d7b2e28c65bff,
  0x0335e56fa1c14599, 0x066bcadf43828b33, 0x0cd795be87051665, 0x19af2b7d0e0a2ccb,
  0x052308b29c686f5c, 0x0a46116538d0deb8, 0x148c22ca71a1bd6f, 0x041c06f549ed25e3,
  0x08380dea93da4bc6, 0x10701bd527b4978c, 0x03499f2aa18a84b6, 0x06933e554315096b,
  0x0d267caa862a12d6, 0x1a4cf9550c5425ad, 0x0542984435aa6def, 0x0a8530886b54dbdf,
  0x150a6110d6a9b7bd, 0x0435469cf7bb8b26, 0x086a8d39ef77164c, 0x10d51a73deee2c98,
  0x035dd2172c9608eb, 0x06bba42e592c11d6, 0x0d77485cb25823ac, 0x1aee90b964b04759,
  0x0562e9beadbcdb12, 0x0ac5d37d5b79b624, 0x158ba6fab6f36c47, 0x044f216557ca48db,
  0x089e42caaf9491b6, 0x113c85955f29236c, 0x0372811ddfd50716, 0x06e5023bbfaa0e2b,
  0x0dca04777f541c56, 0x1b9408eefea838ad, 0x058401c96621a4ef, 0x0b080392cc4349df,
  0x16100725988693be, 0x04699b0784e7b726, 0x08d3360f09cf6e4c, 0x11a66c1e139edc98,
  0x0387af39371fc5b8, 0x070f5e726e3f8b70, 0x0e1ebce4dc7f16e0, 0x02d2f2942c196afa,
  0x05a5e5285832d5f3, 0x0b4bca50b065abe6, 0x169794a160cb57cc, 0x0484b75379c244c2,
  0x09096ea6f3848985, 0x1212dd4de709130a, 0x039d5f75fb01d09c, 0x073abeebf603a137,
  0x0e757dd7ec07426e, 0x02e44c5e6267da16, 0x05c898bcc4cfb42c, 0x0b913179899f6858,
  0x172262f3133ed0b1, 0x04a07a309d72f68a, 0x0940f4613ae5ed13, 0x1281e8c275cbda27,
  0x03b394f3b128c53b, 0x076729e762518a76, 0x0ece53cec4a314ec, 0x02f610c2f4209dc9,
  0x05ec2185e8413b92, 0x0bd8430bd0827723, 0x17b08617a104ee46, 0x04bce79e536762db,
  0x0979cf3ca6cec5b6, 0x12f39e794d9d8b6b, 0x03ca52e50f85e8af, 0x0794a5ca1f0bd15e,
  0x0f294b943e17a2bc, 0x03084250d937ed59, 0x061084a1b26fdab2, 0x0c21094364dfb563,
  0x18421286c9bf6ac7, 0x04da03b48ebfe228, 0x09b407691d7fc450, 0x13680ed23aff889f,
  0x03e19c9072331b53, 0x07c33920e46636a6, 0x0f867241c8cc6d4c, 0x031ae3a6c1c27c42,
  0x0635c74d8384f885, 0x0c6b8e9b0709f10a, 0x18d71d360e13e213, 0x04f7d2a469372d37,
  0x09efa548d26e5a6e, 0x13df4a91a4dcb4dc, 0x03f97550542c242c, 0x07f2eaa0a8584858,
  0x0fe5d54150b090b0, 0x032df7737689b68a, 0x065beee6ed136d13, 0x0cb7ddcdda26da27,
  0x196fbb9bb44db44d, 0x051658b8bda9240f, 0x0a2cb1717b52481f, 0x145962e2f6a4903e,
  0x0411e093caedb673, 0x0823c12795db6ce5, 0x1047824f2bb6d9cb, 0x034180763bf15ec2,
  0x068300ec77e2bd84, 0x0d0601d8efc57b09, 0x1a0c03b1df8af611, 0x05359a56c64efe03,
  0x0a6b34ad8c9dfc07, 0x14d6695b193bf80e, 0x042ae1df050bfe69, 0x0855c3be0a17fcd2,
  0x10ab877c142ff9a5, 0x0355817f373ccb87, 0x06ab02fe6e79970f, 0x0d5605fcdcf32e1d,
  0x1aac0bf9b9e65c3b, 0x05559bfebec7ac0c, 0x0aab37fd7d8f5818, 0x15566ffafb1eb02f,
  0x04447ccbcbd2f009, 0x0888f99797a5e013, 0x1111f32f2f4bc026, 0x0369fd6fd64259a1,
  0x06d3fadfac84b342, 0x0da7f5bf59096685, 0x1b4feb7eb212cd09, 0x0576624c8a03c29b,
  0x0aecc49914078537, 0x15d98932280f0a6e, 0x045eb50a08030216, 0x08bd6a141006042c,
  0x117ad428200c0858, 0x037ef73b399c01ab, 0x06fdee7673380356, 0x0dfbdcece67006ad,
  0x1bf7b9d9cce00d59, 0x0597f1f85c2ccf78, 0x0b2fe3f0b8599ef0, 0x165fc7e170b33de1,
  0x04798e6049bd72c7, 0x08f31cc0937ae58d, 0x11e6398126f5cb1a, 0x039471e6a1645bd2,
  0x0728e3cd42c8b7a4, 0x0e51c79a85916f48, 0x02dd27ebb4504975, 0x05ba4fd768a092ea,
  0x0b749faed14125d3, 0x16e93f5da2824ba7, 0x04950cac53b3a8bb, 0x092a1958a7675176,
  0x125432b14ecea2ec, 0x03aa7089dc8fba2f, 0x0754e113b91f745e, 0x0ea9c227723ee8bd,
  0x02eec06e4a0c94f3, 0x05dd80dc941929e5, 0x0bbb01b9283253ca, 0x177603725064a794,
  0x04b133e3a9adbb1e, 0x096267c7535b763b, 0x12c4cf8ea6b6ec77, 0x03c0f64fbaf1627e,
  0x0781ec9f75e2c4fc, 0x0f03d93eebc589f9, 0x0300c50c958de865, 0x06018a192b1bd0ca,
  0x0c0314325637a194, 0x18062864ac6f4327, 0x04ce0814227ca708, 0x099c102844f94e10,
  0x1338205089f29c1f, 0x03d8067681fd526d, 0x07b00ced03faa4d9, 0x0f6019da07f549b3,
  0x0313385ece6441f1, 0x062670bd9cc883e1, 0x0c4ce17b399107c2, 0x1899c2f673220f84,
  0x04eb8d647d6d364e, 0x09d71ac8fada6c9b, 0x13ae3591f5b4d937, 0x03efa45064575ea5,
  0x07df48a0c8aebd49, 0x0fbe9141915d7a92, 0x03261d0d1d12b21d, 0x064c3a1a3a25643a,
  0x0c987434744ac875, 0x1930e868e89590ea, 0x0509c814fb511cfc, 0x0a139029f6a239f7,
  0x14272053ed4473ee, 0x0407d343fc40e3fc, 0x080fa687f881c7f9, 0x101f4d0ff1038ff2,
  0x033975cffd00b664, 0x0672eb9ffa016cc7, 0x0ce5d73ff402d98e, 0x19cbae7fe805b31c,
  0x0528bc7ffb345706, 0x0a5178fff668ae0b, 0x14a2f1ffecd15c17, 0x042096ccc8f6ac05,
  0x08412d9991ed5809, 0x10825b3323dab012, 0x034d4570a0c5566a, 0x069a8ae1418aacd4,
  0x0d3515c2831559a8, 0x1a6a2b85062ab350, 0x05486f1a9ad55710, 0x0a90de3535aaae20,
  0x1521bc6a6b555c40, 0x0439f27baf111273, 0x0873e4f75e2224e7, 0x10e7c9eebc4449cd,
  0x03618ec958da7529, 0x06c31d92b1b4ea52, 0x0d863b256369d4a4, 0x1b0c764ac6d3a948,
  0x0568e4755af721db, 0x0ad1c8eab5ee43b6, 0x15a391d56bdc876d, 0x0453e9f77bf8e7e3,
  0x08a7d3eef7f1cfc5, 0x114fa7ddefe39f8a, 0x037654c5fcc71fe8, 0x06eca98bf98e3fd1,
  0x0dd95317f31c7fa2, 0x1bb2a62fe638ff44, 0x058a213cc7a4ffda, 0x0b1442798f49ffb5,
  0x162884f31e93ff69, 0x046e80fd6c83ffe2, 0x08dd01fad907ffc4, 0x11ba03f5b20fff87,
  0x038b9a6456cfffe8, 0x071734c8ad9fffd0, 0x0e2e69915b3fffa0, 0x02d6151d123fffed,
  0x05ac2a3a247fffd9, 0x0b58547448ffffb3, 0x16b0a8e891ffff66, 0x0489bb61b6ccccae,
  0x091376c36d99995c, 0x1226ed86db3332b8, 0x03a162b4923d708b, 0x0742c569247ae116,
  0x0e858ad248f5c22d, 0x02e7822a0e978d3c, 0x05cf04541d2f1a78, 0x0b9e08a83a5e34f0,
  0x173c115074bc69e1, 0x04a59d101758e1fa, 0x094b3a202eb1c3f4, 0x129674405d6387e7,
  0x03b7b0d9ac471b2e, 0x076f61b3588e365c, 0x0edec366b11c6cb9, 0x02f95a47bd05af58,
  0x05f2b48f7a0b5eb0, 0x0be5691ef416bd61, 0x17cad23de82d7ac2, 0x04c22a0c61a2b227,
  0x09845418c345644d, 0x1308a831868ac89b, 0x03ce8809e7b55b52, 0x079d1013cf6ab6a4,
  0x0f3a20279ed56d49, 0x030ba007ec9115db, 0x0617400fd9222bb7, 0x0c2e801fb244576d,
  0x185d003f6488aedb, 0x04df6673141b562c, 0x09becce62836ac57, 0x137d99cc506d58af,
  0x03e5eb8f434911bd, 0x07cbd71e86922379, 0x0f97ae3d0d2446f2, 0x031e560c35d40e30,
  0x063cac186ba81c61, 0x0c795830d75038c2, 0x18f2b061aea07184, 0x04fd5679efb9b04e,
};
static void kernel_dragonfly(char **pch, Column *col, int64_t row) {
  char *ch = *pch;
  uint64_t value = ((uint64_t*) col->data)[row];
  if (value & F64_SIGN_MASK) {
    *ch++ = '-';
    value ^= F64_SIGN_MASK;
  }

  int eb = static_cast<int>(value >> 52);
  if (eb == 0x7FF) {
    if (!value) {  // don't print nans at all
      ch[0] = 'i'; ch[1] = 'n'; ch[2] = 'f';
      *pch = ch + 3;
    }
    return;
  } else if (eb == 0x000) {
    *ch++ = '0';
    *pch = ch;
    return;
  }

  int E = ((201 + eb*1233) >> 12) - 308;
  uint64_t F = (value & F64_MANT_MASK) | F64_EXTRA_BIT;
  uint64_t A = Atable[eb];
  unsigned __int128 p = static_cast<unsigned __int128>(F) * static_cast<unsigned __int128>(A);
  int64_t D = static_cast<int64_t>(p >> 53) + (static_cast<int64_t>(p) >> 53);
  if (D >= TENp18) {
    D /= 10;
    E++;
  }
  if (E >= -5 && E < 15) {
    if (E < 0) {
      *ch++ = '0';
      *ch++ = '.';
      for (int r = -E-1; r; r--) {
        *ch++ = '0';
      }
      int r = 17;
      while (D) {
        int64_t d = D / DIVS64[r];
        D -= d * DIVS64[r];
        *ch++ = static_cast<char>(d) + '0';
        r--;
      }
    } else {
      int r = 17;
      int p = r - E;
      while (D) {
        int64_t d = D / DIVS64[r];
        D -= d * DIVS64[r];
        *ch++ = static_cast<char>(d) + '0';
        if (r == p) { *ch++ = '.'; }
        r--;
      }
    }
  } else {
    int64_t d = D / TENp17;
    D -= d * TENp17;
    *ch = static_cast<char>(d) + '0';
    ch[1] = '.';
    ch += 1 + (D != 0);
    int r = 16;
    while (D) {
      d = D / DIVS64[r];
      D -= d * DIVS64[r];
      *ch++ = static_cast<char>(d) + '0';
      r--;
    }
    *ch++ = 'e';
    write_exponent(&ch, E);
  }
  *pch = ch;
}


static void kernel_dragonfly2(char **pch, Column *col, int64_t row) {
  char *ch = *pch;
  uint64_t value = ((uint64_t*) col->data)[row];
  if (value & F64_SIGN_MASK) {
    *ch++ = '-';
    value ^= F64_SIGN_MASK;
  }
  if (value > F64_1em5 && value < F64_1e15) {
    union {uint64_t u; double d;} uvalue = {value};
    double frac, intval;
    frac = modf(uvalue.d, &intval);
    char *ch0 = ch;
    write_int32(&ch, static_cast<int32_t>(intval));

    if (frac) {
      int digits_left = 15 - (ch - ch0);
      *ch++ = '.';
      while (frac > 0 && digits_left) {
        frac *= 10;
        frac = modf(frac, &intval);
        *ch++ = static_cast<char>(intval) + '0';
        digits_left--;
      }
      if (!digits_left) {
        intval = frac*10 + 0.5;
        if (intval > 9) intval = 9;
        *ch++ = static_cast<char>(intval) + '0';
      }
    }
    *pch = ch;
    return;
  }

  int eb = static_cast<int>(value >> 52);
  if (eb == 0x7FF) {
    if (!value) {  // don't print nans at all
      *ch++ = 'i'; *ch++ = 'n'; *ch++ = 'f';
      *pch = ch;
    }
    return;
  } else if (eb == 0x000) {
    *ch++ = '0';
    *pch = ch;
    return;
  }
  int E = ((201 + eb*1233) >> 12) - 308;
  uint64_t F = (value & F64_MANT_MASK) | F64_EXTRA_BIT;
  uint64_t A = Atable[eb];
  unsigned __int128 p = static_cast<unsigned __int128>(F) * static_cast<unsigned __int128>(A);
  int64_t D = static_cast<int64_t>(p >> 53) + (static_cast<int64_t>(p) >> 53);
  if (D >= TENp18) {
    D /= 10;
    E++;
  }
  ch += 18;
  char *tch = ch;
  for (int r = 18; r; r--) {
    ldiv_t ddd = ldiv(D, 10);
    D = ddd.quot;
    *tch-- = static_cast<char>(ddd.rem) + '0';
    if (r == 2) { *tch-- = '.'; }
  }
  while (ch[-1] == '0') ch--;  // remove any trailing 0s, for the sake of compactness
  *ch++ = 'e';
  write_exponent(&ch, E);
  *pch = ch;
}


//=================================================================================================
// Main
//=================================================================================================

BenchmarkSuite prepare_bench_double(int64_t N)
{
  Column *column = (Column*) malloc(sizeof(Column));

  // const double LOG_2_10 = log2(10.0);
  // const long double LOG_2_10L = log2l(10.0L);
  // for (int eb = 0; eb < 0x7FF; eb++) {
  //   int E = ((201 + eb*1233) >> 12) - 308;
  //   long double Ald = powl(static_cast<long double>(2), eb - 0x3FF + 1 + LOG_2_10L*(17 - E));
  //   uint64_t Au2 = static_cast<uint64_t>(floorl(Ald));
  //   Atable[eb] = static_cast<uint64_t>(floorl(Ald));
  // }
  printf("Atable[0x45D] = %llu\n", Atable[0x45D]);
  // Atable[0x45D] = 337350334183376743;
  char *test = new char[1000]; test[25] = '\0';
  uint64_t testval[] = { 0x45DFE640D407A3B5ull };
  column->data = (void*)testval;
  char *ch = test;
  kernel_dragonfly2(&ch, column, 0);
  printf("  0x45DFE640D407A3B5 => %s\n", test);

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
  column->data = (void*)data;

  static Kernel kernels[] = {
    { &kernel_mixed,     "mixed" },       // 220.984
    { &kernel_altmixed,  "altmixed" },    // 207.389
    { &kernel_miloyip,   "miloyip" },     // 255.513
    { &kernel_dragonfly, "dragonfly" },   // 259.139
    { &kernel_dragonfly2,"dragonfly2" },  // 209.373
    { &kernel_hex,       "hex" },         //  76.346
    { &kernel_fwrite,    "fwrite" },      // 363.001
    { &kernel_sprintf,   "sprintf" },     // 620.818
    { NULL, NULL },
  };

  return {
    .column = column,
    .output = out,
    .kernels = kernels,
  };
}

// Verified with WolframAlpha and Java:
// -0x1.05d04dee8ba09p+9   => 0xB0805D04DEE8bA09 => -523.6273782903455 (1850782...)
//  0x1.9be5884b37cb1p-2   => 0x3FD9BE5884B37CB1 => 0.40224278411001096 (467103...)
//  0x1.7ff44ce5ffe8ap+2   => 0x4017FF44CE5FFE8A => 5.999285912140872 (2->1701581...)
//  0x1.b8ace99ab159dp+8   => 0x407B8ACE99AB159D => 440.67543951825957 (7->658298...)
//  0x1.b8ace99ab159ep+8   => 0x407B8ACE99AB159E => 440.6754395182596 (226732...)
//  0x1.3099cb1720a7dp+125 => 0x47C3099CB1720A7D => 5.061048141243424e+37 (0785685...)
//  0x1.80a72c43a9beap-393 => 0x27680A72C43A9BEA => 7.448020860295801e-119 (3458037...)
//  0x1.9a7248c89015cp-265 => 0x2F69A7248C89015C => 2.7043796482982135e-80 (3071006...)
// -0x1.9ccb4a4f89969p+9   => 0xC089CCB4A4F89969 => -825.5882052823846 (6->5924649...)
//  0x1.fe640d407a3b5p+94  => 0x45DFE640D407A3B5 => 3.9489577542752767e+28 (7->686177...)
