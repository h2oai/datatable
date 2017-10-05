#include <stdint.h>  // int8_t, ...
#include <stdio.h>   // printf
#include <stdlib.h>  // srand, rand
#include <time.h>    // time
#include <math.h>    // isna
#include "writecsv.h"


#define F32_SIGN_MASK  0x80000000
#define F32_INFINITY   0x7F800000
#define TENp08         100000000
#define TENp09         1000000000
typedef union { float f;  uint32_t u; }  _flt_u32;
static const int32_t DIVS32[10] = {
  1,
  10,
  100,
  1000,
  10000,
  100000,
  1000000,
  10000000,
  100000000,
  1000000000,
};
static const uint32_t Atable32[256] = {
  0x46109ecf, 0x0e0352f6, 0x1c06a5ec, 0x380d4bd9,
  0x701a97b1, 0x166bb7f0, 0x2cd76fe1, 0x59aedfc1,
  0x11efc65a, 0x23df8cb4, 0x47bf1967, 0x0e596b7b,
  0x1cb2d6f6, 0x3965adec, 0x72cb5bd8, 0x16f578c5,
  0x2deaf18a, 0x5bd5e314, 0x125dfa37, 0x24bbf46e,
  0x4977e8dc, 0x0eb194f9, 0x1d6329f2, 0x3ac653e4,
  0x758ca7c7, 0x178287f5, 0x2f050fe9, 0x5e0a1fd2,
  0x12ced32a, 0x259da654, 0x4b3b4ca8, 0x0f0bdc22,
  0x1e17b843, 0x3c2f7087, 0x0c097ce8, 0x1812f9cf,
  0x3025f39f, 0x604be73e, 0x13426173, 0x2684c2e6,
  0x4d0985cb, 0x0f684df5, 0x1ed09beb, 0x3da137d6,
  0x0c537191, 0x18a6e322, 0x314dc645, 0x629b8c89,
  0x13b8b5b5, 0x27716b6a, 0x4ee2d6d4, 0x0fc6f7c4,
  0x1f8def88, 0x3f1bdf10, 0x0c9f2c9d, 0x193e593a,
  0x327cb273, 0x64f964e7, 0x1431e0fb, 0x2863c1f6,
  0x50c783ec, 0x1027e72f, 0x204fce5e, 0x409f9cbc,
  0x0cecb8f2, 0x19d971e5, 0x33b2e3ca, 0x6765c794,
  0x14adf4b7, 0x295be96e, 0x52b7d2dd, 0x108b2a2c,
  0x21165458, 0x422ca8b1, 0x0d3c21bd, 0x1a78437a,
  0x34f086f4, 0x69e10de7, 0x152d02c8, 0x2a5a0590,
  0x54b40b20, 0x10f0cf06, 0x21e19e0d, 0x43c33c19,
  0x0d8d726b, 0x1b1ae4d7, 0x3635c9ae, 0x6c6b935c,
  0x15af1d79, 0x2b5e3af1, 0x56bc75e3, 0x1158e461,
  0x22b1c8c1, 0x45639182, 0x0de0b6b4, 0x1bc16d67,
  0x3782dacf, 0x6f05b59d, 0x16345786, 0x2c68af0c,
  0x58d15e17, 0x11c37938, 0x2386f270, 0x470de4e0,
  0x0e35fa93, 0x1c6bf526, 0x38d7ea4c, 0x71afd499,
  0x16bcc41f, 0x2d79883d, 0x5af3107a, 0x12309ce5,
  0x246139ca, 0x48c27395, 0x0e8d4a51, 0x1d1a94a2,
  0x3a352944, 0x746a5288, 0x174876e8, 0x2e90edd0,
  0x5d21dba0, 0x12a05f20, 0x2540be40, 0x4a817c80,
  0x0ee6b280, 0x1dcd6500, 0x3b9aca00, 0x0bebc200,
  0x17d78400, 0x2faf0800, 0x5f5e1000, 0x1312d000,
  0x2625a000, 0x4c4b4000, 0x0f424000, 0x1e848000,
  0x3d090000, 0x0c350000, 0x186a0000, 0x30d40000,
  0x61a80000, 0x13880000, 0x27100000, 0x4e200000,
  0x0fa00000, 0x1f400000, 0x3e800000, 0x0c800000,
  0x19000000, 0x32000000, 0x64000000, 0x14000000,
  0x28000000, 0x50000000, 0x10000000, 0x20000000,
  0x40000000, 0x0ccccccd, 0x1999999a, 0x33333333,
  0x66666666, 0x147ae148, 0x28f5c28f, 0x51eb851f,
  0x10624dd3, 0x20c49ba6, 0x4189374c, 0x0d1b7176,
  0x1a36e2eb, 0x346dc5d6, 0x68db8bac, 0x14f8b589,
  0x29f16b12, 0x53e2d624, 0x10c6f7a1, 0x218def41,
  0x431bde83, 0x0d6bf94d, 0x1ad7f29b, 0x35afe535,
  0x6b5fca6b, 0x15798ee2, 0x2af31dc4, 0x55e63b89,
  0x112e0be8, 0x225c17d0, 0x44b82fa1, 0x0dbe6fed,
  0x1b7cdfda, 0x36f9bfb4, 0x6df37f67, 0x15fd7fe1,
  0x2bfaffc3, 0x57f5ff86, 0x11979981, 0x232f3302,
  0x465e6605, 0x0e12e134, 0x1c25c268, 0x384b84d1,
  0x709709a1, 0x16849b87, 0x2d09370d, 0x5a126e1b,
  0x1203af9f, 0x24075f3e, 0x480ebe7c, 0x0e69594c,
  0x1cd2b298, 0x39a56530, 0x734aca5f, 0x170ef546,
  0x2e1dea8d, 0x5c3bd519, 0x12725dd2, 0x24e4bba4,
  0x49c97747, 0x0ec1e4a8, 0x1d83c950, 0x3b07929f,
  0x760f253f, 0x179ca10d, 0x2f394219, 0x5e728432,
  0x12e3b40a, 0x25c76814, 0x4b8ed028, 0x0f1c9008,
  0x1e392010, 0x3c724020, 0x0c16d9a0, 0x182db340,
  0x305b6680, 0x60b6cd00, 0x1357c29a, 0x26af8533,
  0x4d5f0a67, 0x0f79687b, 0x1ef2d0f6, 0x3de5a1ec,
  0x0c612062, 0x18c240c5, 0x31848189, 0x63090313,
  0x13ce9a37, 0x279d346e, 0x4f3a68dc, 0x0fd87b5f,
  0x1fb0f6be, 0x3f61ed7d, 0x0cad2f7f, 0x195a5eff,
  0x32b4bdfd, 0x65697bfb, 0x14484bff, 0x289097fe,
};



static void kernel_sprintf(char **pch, Column *col, int64_t row) {
  float value = ((float*) col->data)[row];
  if (isnan(value)) return;
  *pch += sprintf(*pch, "%.9g", value);
}


inline void ftoa(char **pch, float fvalue)
{
  char *ch = *pch;
  uint32_t value = ((_flt_u32){ .f = fvalue }).u;

  if (value & F32_SIGN_MASK) {
    *ch++ = '-';
    value ^= F32_SIGN_MASK;
  }
  int eb = static_cast<int>(value >> 23);  // biased exponent
  if (eb == 0xFF) {
    if (value == F32_INFINITY) {  // don't print nans at all
      ch[0] = 'i'; ch[1] = 'n'; ch[2] = 'f';
      *pch = ch + 3;
    }
    return;
  } else if (eb == 0x00) {
    *ch++ = '0';
    *pch = ch;
    return;
  }

  // Main part of the algorithm: compute D and E
  int E = ((3153 + eb*1233) >> 12) - 39;
  uint32_t G = (value << 8) | F32_SIGN_MASK;
  uint32_t A = Atable32[eb];
  uint64_t p = static_cast<uint64_t>(G) * static_cast<uint64_t>(A);
  int32_t D = static_cast<int32_t>((p + F32_SIGN_MASK) >> 32);
  int32_t eps = static_cast<int32_t>(A >> 25);
  printf("  D = %d, A = %u, G = %u, eps = %d\n", D, A, G, eps);

  // Round the value of D according to its precision
  int mod = 1000;
  int rem = static_cast<int>(D % 1000);
  while (mod > 1) {
    printf("  -> mod = %d, rem = %d\n", mod, rem);
    if (eps >= rem) {
      D = D - rem + (rem > mod/2) * mod;
      break;
    } else if (eps >= mod - rem) {
      D = D - rem + mod;
      break;
    }
    mod /= 10;
    rem %= mod;
  }
  printf("  -> D = %d\n", D);
  bool bigD = (D >= TENp09);
  int EE = E + bigD;

  // Write the decimal number into the buffer, in one of the three formats
  // depending on the magnitude of E.
  if (EE < -4 || EE > 7) {
    // Small/large numbers write in scientific notation: 1.2345e+67
    int32_t d = D / TENp08;
    D -= d * TENp08;
    if (bigD) {
      int32_t dd = d / 10;
      d -= dd * 10;
      *ch++ = static_cast<char>(dd) + '0';
      *ch++ = '.';
      *ch++ = static_cast<char>(d) + '0';
      if (!d && !D) ch -= 2;
    } else {
      *ch++ = static_cast<char>(d) + '0';
      *ch = '.';
      ch += (D != 0);
    }
    int r = 7;
    while (D) {
      d = D / DIVS32[r];
      D -= d * DIVS32[r];
      *ch++ = static_cast<char>(d) + '0';
      r--;
    }
    // Write exponent. This code will output the integer number `E` as two
    // digits always: 12, 05, 38. In practice |E| ≤ 38, so two digits is
    // enough.
    *ch++ = 'e';
    if (EE < 0) {
      *ch++ = '-';
      EE = -EE;
    } else {
      *ch++ = '+';
    }
    int q = EE / 10;
    *ch++ = static_cast<char>(q) + '0';
    *ch++ = static_cast<char>(EE - q*10) + '0';
  } else if (EE < 0) {
    // Numbers less than one, use floating point format: 0.000123456789
    // Note: we use threshold 1e-4 to determine whether to write the
    // number in this format. Any lower threshold would increase the maximum
    // possible length of the produced string.
    *ch++ = '0';
    *ch++ = '.';
    for (int r = -EE-1; r; r--) {
      *ch++ = '0';
    }
    int r = 8 + bigD;
    while (D) {
      int32_t d = D / DIVS32[r];
      D -= d * DIVS32[r];
      *ch++ = static_cast<char>(d) + '0';
      r--;
    }
  } else {
    // Numbers greater than one, use floating point format: 12345.67
    int r = 8 + bigD;
    int rr = r - EE;
    while (D || r >= rr) {
      int32_t d = D / DIVS32[r];
      D -= d * DIVS32[r];
      *ch++ = static_cast<char>(d) + '0';
      if (r == rr && D) { *ch++ = '.'; }
      r--;
    }
  }
  *pch = ch;
}

static void kernel_dragonfly32(char **pch, Column *col, int64_t row) {
  float value = ((float*) col->data)[row];
  ftoa(pch, value);
}

static void kernel_dragonfly64(char **pch, Column *col, int64_t row) {
  double value = (double) ((float*) col->data)[row];
  static Column fakecol;
  fakecol.data = &value;
  kernel_dragonflyD(pch, &fakecol, 0);
}

static inline void write_int32(char **pch, int32_t value) {
  if (value == 0) {
    *((*pch)++) = '0';
    return;
  }
  char *ch = *pch;
  if (value < 0) {
    *ch++ = '-';
    value = -value;
  }
  int r = (value < 100000)? 4 : 9;
  for (; value < DIVS32[r]; r--);
  for (; r; r--) {
    int d = value / DIVS32[r];
    *ch++ = static_cast<char>(d) + '0';
    value -= d * DIVS32[r];
  }
  *ch = static_cast<char>(value) + '0';
  *pch = ch + 1;
}

static char hexdigits[] = {'0', '1', '2', '3', '4', '5', '6', '7',
                           '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};
static void kernel_hex(char **pch, Column *col, int64_t row)
{
  // Read the value as if it was uint32_t
  uint32_t value = static_cast<uint32_t*>(col->data)[row];
  char *ch = *pch;

  int exp = static_cast<int>(value >> 23);
  uint32_t sig = (value & 0x7FFFFF);
  if (exp & 0x100) {
    *ch++ = '-';
    exp ^= 0x100;
  }
  if (exp == 0xFF) {  // nan & inf
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
  ch[2] = '0' + (exp != 0x00);
  ch[3] = '.';
  ch += 3 + (sig != 0);
  while (sig) {
    uint32_t r = sig & 0x780000;
    *ch++ = hexdigits[r >> 19];
    sig = (sig ^ r) << 4;
  }
  if (exp) exp -= 0x7F;
  *ch++ = 'p';
  *ch++ = '+' + (exp < 0)*('-' - '+');
  write_int32(&ch, abs(exp));
  *pch = ch;
}


//=================================================================================================
// Main
//=================================================================================================

BenchmarkSuite prepare_bench_float(int64_t N)
{
  Column *column = (Column*) malloc(sizeof(Column));

  // Prepare data array
  srand((unsigned) time(NULL));
  float *data = (float*) malloc(N * sizeof(float));
  for (int64_t i = 0; i < N; i++) {
    int t = rand();
    float x = (float)(rand()) / RAND_MAX;
    data[i] = (t&15)<=1? NA_F4 :
              (t&15)==2? 0 :
              (t&15)==3? x :
              (t&15)==4? x * 100 :
              (t&15)==5? x * 10000 :
              (t&15)==6? -x :
              (t&15)==7? -10 * x :
              (t&15)==8? -1000 * x :
              (t&15)<=12? x * pow(10, 5 + t % 32) * (1 - 2*(t&1)) :
                          x * pow(0.1, 5 + t % 32) * (1 - 2*(t&1));
  }
  data[0] = 1000000.2;

  // Prepare output buffer
  // At most 25 characters per entry (e.g. '-1.3456789011111343e+123') + 1 for a comma
  char *out = (char*) malloc((N + 1) * 20);
  column->data = (void*)data;

  static Kernel kernels[] = {
    { &kernel_hex,         "hex" },
    // { &kernel_dragonfly64, "dragonfly64" },
    { &kernel_dragonfly32, "dragonfly32" },
    { &kernel_sprintf,     "sprintf" },
    { NULL, NULL },
  };

  return {
    .column = column,
    .output = out,
    .kernels = kernels,
  };
}
