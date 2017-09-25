//------------------------------------------------------------------------------
// This code was adapted from
//   https://github.com/miloyip/dtoa-benchmark/blob/master/src/milo/dtoa_milo.h
// (MIT licensed)
//
// Copyright (C) 2014 Milo Yip
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//------------------------------------------------------------------------------
#ifndef dt_CSV_DTOA_H
#define dt_CSV_DTOA_H
#include <math.h>
#include <string.h>
#include "csv.h"

#if defined(_MSC_VER)
  #include "msinttypes/stdint.h"
  #include <intrin.h>
#else
  #include <stdint.h>
#endif

#define UINT64_C2(h, l) ((static_cast<uint64_t>(h) << 32) | static_cast<uint64_t>(l))

class DiyFp64 {
public:
  uint64_t f;
  int e;
  int _padding;

  static const int kDiySignificandSize = 64;
  static const int kDpSignificandSize = 52;
  static const int kDpExponentBias = 0x3FF + kDpSignificandSize;
  static const uint64_t kDpExponentMask    = 0x7FF0000000000000;
  static const uint64_t kDpSignificandMask = 0x000FFFFFFFFFFFFF;
  static const uint64_t kDpHiddenBit       = 0x0010000000000000;

  // Methods & operators
  DiyFp64() {}
  DiyFp64(uint64_t f_, int e_) : f(f_), e(e_) {}

  DiyFp64(double d) {
    union { double d; uint64_t u64; } u = { d };
    int biased_e = (u.u64 >> 52) & 0x7FF;
    uint64_t significand = (u.u64 & kDpSignificandMask);
    int normal = (biased_e != 0);
    f = significand + (static_cast<uint64_t>(normal) << 52);
    e = biased_e - kDpExponentBias + 1 - normal;
  }

  DiyFp64 operator-(const DiyFp64& rhs) const {
    // assert(e == rhs.e);
    // assert(f >= rhs.f);
    return DiyFp64(f - rhs.f, e);
  }

  DiyFp64 operator*(const DiyFp64& rhs) const {
    #if defined(_MSC_VER) && defined(_M_AMD64)
      uint64_t h;
      uint64_t l = _umul128(f, rhs.f, &h);
      if (l & (uint64_t(1) << 63)) h++; // rounding
      return DiyFp64(h, e + rhs.e + 64);
    #elif (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6)) && defined(__x86_64__)
      unsigned __int128 p = static_cast<unsigned __int128>(f) * static_cast<unsigned __int128>(rhs.f);
      uint64_t h = p >> 64;
      uint64_t l = static_cast<uint64_t>(p);
      if (l & (uint64_t(1) << 63)) h++; // rounding
      return DiyFp64(h, e + rhs.e + 64);
    #else
      const uint64_t a = f >> 32;
      const uint64_t b = f & 0xFFFFFFFF;
      const uint64_t c = rhs.f >> 32;
      const uint64_t d = rhs.f & 0xFFFFFFFF;
      const uint64_t ac = a * c;
      const uint64_t bc = b * c;
      const uint64_t ad = a * d;
      const uint64_t bd = b * d;
      uint64_t tmp = (bd >> 32) + (ad & 0xFFFFFFFF) + (bc & 0xFFFFFFFF);
      tmp += 1U << 31;  /// mult_round
      return DiyFp64(ac + (ad >> 32) + (bc >> 32) + (tmp >> 32), e + rhs.e + 64);
    #endif
  }

  DiyFp64 Normalize() const {
    #if defined(_MSC_VER) && defined(_M_AMD64)
      unsigned long index;
      _BitScanReverse64(&index, f);
      return DiyFp64(f << (63 - index), e - (63 - index));
    #elif defined(__GNUC__)
      int s = __builtin_clzll(f);
      return DiyFp64(f << s, e - s);
    #else
      DiyFp64 res = *this;
      while (!(res.f & kDpHiddenBit)) {
        res.f <<= 1;
        res.e--;
      }
      res.f <<= (kDiySignificandSize - kDpSignificandSize - 1);
      res.e = res.e - (kDiySignificandSize - kDpSignificandSize - 1);
      return res;
    #endif
  }

  DiyFp64 NormalizeBoundary() const {
    #if defined(_MSC_VER) && defined(_M_AMD64)
      unsigned long index;
      _BitScanReverse64(&index, f);
      return DiyFp64 (f << (63 - index), e - (63 - index));
    #else
      DiyFp64 res = *this;
      while (!(res.f & (kDpHiddenBit << 1))) {
        res.f <<= 1;
        res.e--;
      }
      res.f <<= (kDiySignificandSize - kDpSignificandSize - 2);
      res.e = res.e - (kDiySignificandSize - kDpSignificandSize - 2);
      return res;
    #endif
  }

  void NormalizedBoundaries(DiyFp64* minus, DiyFp64* plus) const {
    DiyFp64 pl = DiyFp64((f << 1) + 1, e - 1).NormalizeBoundary();
    DiyFp64 mi = (f == kDpHiddenBit) ? DiyFp64((f << 2) - 1, e - 2) : DiyFp64((f << 1) - 1, e - 1);
    mi.f <<= mi.e - pl.e;
    mi.e = pl.e;
    *plus = pl;
    *minus = mi;
  }
};


static inline DiyFp64 GetCachedPower(int e, int* K) {
  // 10^-348, 10^-340, ..., 10^340
  static const uint64_t kCachedPowers_F[] = {
    UINT64_C2(0xfa8fd5a0, 0x081c0288), UINT64_C2(0xbaaee17f, 0xa23ebf76),
    UINT64_C2(0x8b16fb20, 0x3055ac76), UINT64_C2(0xcf42894a, 0x5dce35ea),
    UINT64_C2(0x9a6bb0aa, 0x55653b2d), UINT64_C2(0xe61acf03, 0x3d1a45df),
    UINT64_C2(0xab70fe17, 0xc79ac6ca), UINT64_C2(0xff77b1fc, 0xbebcdc4f),
    UINT64_C2(0xbe5691ef, 0x416bd60c), UINT64_C2(0x8dd01fad, 0x907ffc3c),
    UINT64_C2(0xd3515c28, 0x31559a83), UINT64_C2(0x9d71ac8f, 0xada6c9b5),
    UINT64_C2(0xea9c2277, 0x23ee8bcb), UINT64_C2(0xaecc4991, 0x4078536d),
    UINT64_C2(0x823c1279, 0x5db6ce57), UINT64_C2(0xc2109436, 0x4dfb5637),
    UINT64_C2(0x9096ea6f, 0x3848984f), UINT64_C2(0xd77485cb, 0x25823ac7),
    UINT64_C2(0xa086cfcd, 0x97bf97f4), UINT64_C2(0xef340a98, 0x172aace5),
    UINT64_C2(0xb23867fb, 0x2a35b28e), UINT64_C2(0x84c8d4df, 0xd2c63f3b),
    UINT64_C2(0xc5dd4427, 0x1ad3cdba), UINT64_C2(0x936b9fce, 0xbb25c996),
    UINT64_C2(0xdbac6c24, 0x7d62a584), UINT64_C2(0xa3ab6658, 0x0d5fdaf6),
    UINT64_C2(0xf3e2f893, 0xdec3f126), UINT64_C2(0xb5b5ada8, 0xaaff80b8),
    UINT64_C2(0x87625f05, 0x6c7c4a8b), UINT64_C2(0xc9bcff60, 0x34c13053),
    UINT64_C2(0x964e858c, 0x91ba2655), UINT64_C2(0xdff97724, 0x70297ebd),
    UINT64_C2(0xa6dfbd9f, 0xb8e5b88f), UINT64_C2(0xf8a95fcf, 0x88747d94),
    UINT64_C2(0xb9447093, 0x8fa89bcf), UINT64_C2(0x8a08f0f8, 0xbf0f156b),
    UINT64_C2(0xcdb02555, 0x653131b6), UINT64_C2(0x993fe2c6, 0xd07b7fac),
    UINT64_C2(0xe45c10c4, 0x2a2b3b06), UINT64_C2(0xaa242499, 0x697392d3),
    UINT64_C2(0xfd87b5f2, 0x8300ca0e), UINT64_C2(0xbce50864, 0x92111aeb),
    UINT64_C2(0x8cbccc09, 0x6f5088cc), UINT64_C2(0xd1b71758, 0xe219652c),
    UINT64_C2(0x9c400000, 0x00000000), UINT64_C2(0xe8d4a510, 0x00000000),
    UINT64_C2(0xad78ebc5, 0xac620000), UINT64_C2(0x813f3978, 0xf8940984),
    UINT64_C2(0xc097ce7b, 0xc90715b3), UINT64_C2(0x8f7e32ce, 0x7bea5c70),
    UINT64_C2(0xd5d238a4, 0xabe98068), UINT64_C2(0x9f4f2726, 0x179a2245),
    UINT64_C2(0xed63a231, 0xd4c4fb27), UINT64_C2(0xb0de6538, 0x8cc8ada8),
    UINT64_C2(0x83c7088e, 0x1aab65db), UINT64_C2(0xc45d1df9, 0x42711d9a),
    UINT64_C2(0x924d692c, 0xa61be758), UINT64_C2(0xda01ee64, 0x1a708dea),
    UINT64_C2(0xa26da399, 0x9aef774a), UINT64_C2(0xf209787b, 0xb47d6b85),
    UINT64_C2(0xb454e4a1, 0x79dd1877), UINT64_C2(0x865b8692, 0x5b9bc5c2),
    UINT64_C2(0xc83553c5, 0xc8965d3d), UINT64_C2(0x952ab45c, 0xfa97a0b3),
    UINT64_C2(0xde469fbd, 0x99a05fe3), UINT64_C2(0xa59bc234, 0xdb398c25),
    UINT64_C2(0xf6c69a72, 0xa3989f5c), UINT64_C2(0xb7dcbf53, 0x54e9bece),
    UINT64_C2(0x88fcf317, 0xf22241e2), UINT64_C2(0xcc20ce9b, 0xd35c78a5),
    UINT64_C2(0x98165af3, 0x7b2153df), UINT64_C2(0xe2a0b5dc, 0x971f303a),
    UINT64_C2(0xa8d9d153, 0x5ce3b396), UINT64_C2(0xfb9b7cd9, 0xa4a7443c),
    UINT64_C2(0xbb764c4c, 0xa7a44410), UINT64_C2(0x8bab8eef, 0xb6409c1a),
    UINT64_C2(0xd01fef10, 0xa657842c), UINT64_C2(0x9b10a4e5, 0xe9913129),
    UINT64_C2(0xe7109bfb, 0xa19c0c9d), UINT64_C2(0xac2820d9, 0x623bf429),
    UINT64_C2(0x80444b5e, 0x7aa7cf85), UINT64_C2(0xbf21e440, 0x03acdd2d),
    UINT64_C2(0x8e679c2f, 0x5e44ff8f), UINT64_C2(0xd433179d, 0x9c8cb841),
    UINT64_C2(0x9e19db92, 0xb4e31ba9), UINT64_C2(0xeb96bf6e, 0xbadf77d9),
    UINT64_C2(0xaf87023b, 0x9bf0ee6b)
  };
  static const int16_t kCachedPowers_E[] = {
    -1220, -1193, -1166, -1140, -1113, -1087, -1060, -1034, -1007,  -980,
     -954,  -927,  -901,  -874,  -847,  -821,  -794,  -768,  -741,  -715,
     -688,  -661,  -635,  -608,  -582,  -555,  -529,  -502,  -475,  -449,
     -422,  -396,  -369,  -343,  -316,  -289,  -263,  -236,  -210,  -183,
     -157,  -130,  -103,   -77,   -50,   -24,     3,    30,    56,    83,
      109,   136,   162,   189,   216,   242,   269,   295,   322,   348,
      375,   402,   428,   455,   481,   508,   534,   561,   588,   614,
      641,   667,   694,   720,   747,   774,   800,   827,   853,   880,
      907,   933,   960,   986,  1013,  1039,  1066
  };

  double dk = (-61 - e) * 0.30102999566398114 + 347;  // dk must be positive, so can do ceiling in positive
  int k = static_cast<int>(dk);
  if (k != dk) k++;

  unsigned index = static_cast<unsigned>((k >> 3) + 1);
  *K = -(-348 + static_cast<int>(index << 3));    // decimal exponent no need lookup table

  // assert(index < sizeof(kCachedPowers_F) / sizeof(kCachedPowers_F[0]));
  return DiyFp64(kCachedPowers_F[index], kCachedPowers_E[index]);
}


static inline void GrisuRound(char* buffer, int len, uint64_t delta, uint64_t rest, uint64_t ten_kappa, uint64_t wp_w) {
  while (rest < wp_w && delta - rest >= ten_kappa &&
         (rest + ten_kappa < wp_w ||  /// closer
          wp_w - rest > rest + ten_kappa - wp_w)) {
    buffer[len - 1]--;
    rest += ten_kappa;
  }
}

static inline unsigned CountDecimalDigit32(uint32_t n) {
  // Simple pure C++ implementation was faster than __builtin_clz version in this situation.
  if (n < 10) return 1;
  if (n < 100) return 2;
  if (n < 1000) return 3;
  if (n < 10000) return 4;
  if (n < 100000) return 5;
  if (n < 1000000) return 6;
  if (n < 10000000) return 7;
  if (n < 100000000) return 8;
  if (n < 1000000000) return 9;
  return 10;
}

static inline void DigitGen(const DiyFp64& W, const DiyFp64& Mp, uint64_t delta, char* buffer, int* len, int* K) {
  static const uint32_t kPow10[] = {1, 10, 100, 1000, 10000, 100000, 1000000, 10000000, 100000000, 1000000000};
  const DiyFp64 one(uint64_t(1) << -Mp.e, Mp.e);
  const DiyFp64 wp_w = Mp - W;
  uint32_t p1 = static_cast<uint32_t>(Mp.f >> -one.e);
  uint64_t p2 = Mp.f & (one.f - 1);
  int kappa = static_cast<int>(CountDecimalDigit32(p1));
  *len = 0;

  while (kappa > 0) {
    uint32_t d;
    switch (kappa) {
      case 10: d = p1 / 1000000000; p1 %= 1000000000; break;
      case  9: d = p1 /  100000000; p1 %=  100000000; break;
      case  8: d = p1 /   10000000; p1 %=   10000000; break;
      case  7: d = p1 /    1000000; p1 %=    1000000; break;
      case  6: d = p1 /     100000; p1 %=     100000; break;
      case  5: d = p1 /      10000; p1 %=      10000; break;
      case  4: d = p1 /       1000; p1 %=       1000; break;
      case  3: d = p1 /        100; p1 %=        100; break;
      case  2: d = p1 /         10; p1 %=         10; break;
      case  1: d = p1;              p1 =           0; break;
      default:
        #if defined(_MSC_VER)
          __assume(0);
        #elif __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 5)
          __builtin_unreachable();
        #else
          d = 0;
        #endif
    }
    if (d || *len) {
      buffer[(*len)++] = '0' + static_cast<char>(d);
    }
    kappa--;
    uint64_t tmp = (static_cast<uint64_t>(p1) << -one.e) + p2;
    if (tmp <= delta) {
      *K += kappa;
      GrisuRound(buffer, *len, delta, tmp, static_cast<uint64_t>(kPow10[kappa]) << -one.e, wp_w.f);
      return;
    }
  }

  // kappa = 0
  for (;;) {
    p2 *= 10;
    delta *= 10;
    char d = static_cast<char>(p2 >> -one.e);
    if (d || *len) {
      buffer[(*len)++] = '0' + d;
    }
    p2 &= one.f - 1;
    kappa--;
    if (p2 < delta) {
      *K += kappa;
      GrisuRound(buffer, *len, delta, p2, one.f, wp_w.f * kPow10[-kappa]);
      return;
    }
  }
}

inline void Grisu2(double value, char* buffer, int* length, int* K) {
  const DiyFp64 v(value);
  DiyFp64 w_m, w_p;
  v.NormalizedBoundaries(&w_m, &w_p);

  const DiyFp64 c_mk = GetCachedPower(w_p.e, K);
  const DiyFp64 W = v.Normalize() * c_mk;
  DiyFp64 Wp = w_p * c_mk;
  DiyFp64 Wm = w_m * c_mk;
  Wm.f++;
  Wp.f--;
  DigitGen(W, Wp, Wp.f - Wm.f, buffer, length, K);
}

#endif
