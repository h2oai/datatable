//------------------------------------------------------------------------------
// Copyright 2018-2023 H2O.ai
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the "Software"),
// to deal in the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense,
// and/or sell copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.
//------------------------------------------------------------------------------
#include "read/constants.h"              // hexdigits, pow10lookup
#include "read/field64.h"                // field64
#include "read/parse_context.h"          // ParseContext
#include "read/parsers/info.h"
#include "utils/tests.h"
namespace dt {
namespace read {

static constexpr uint32_t NA_FLOAT32_I32 = 0x7F8007A2;
static constexpr uint64_t NA_FLOAT64_I64 = 0x7FF000000000DEADull;
static constexpr uint32_t INF_FLOAT32_I32 = 0x7F800000;
static constexpr uint64_t INF_FLOAT64_I64 = 0x7FF0000000000000ull;



//------------------------------------------------------------------------------
// Float32/hex
//------------------------------------------------------------------------------

static void parse_float32_hex(const ParseContext& ctx) {
  const char* ch = ctx.ch;
  uint32_t neg = 0;
  uint8_t digit;
  bool Eneg = 0, subnormal = 0;

  if (ch < ctx.eof) {
    ch += (neg = (*ch=='-')) + (*ch=='+');
  }

  if (ch + 2 < ctx.eof && ch[0]=='0' && (ch[1]=='x' || ch[1]=='X') &&
      (ch[2]=='1' || (subnormal = (ch[2]=='0')) != 0)) {
    ch += 3;
    uint32_t acc = 0;
    if (ch < ctx.eof && *ch == '.') {
      ch++;
      int ndigits = 0;
      while (ch < ctx.eof && (digit = dt::read::hexdigits[static_cast<uint8_t>(*ch)]) < 16) {
        acc = (acc << 4) + digit;
        ch++;
        ndigits++;
      }
      if (ndigits > 6) goto fail;
      acc <<= 24 - ndigits * 4;
      acc >>= 1;
    }
    if ((ch < ctx.eof && *ch!='p' && *ch!='P') || ch == ctx.eof) goto fail;

    if (ch + 1 < ctx.eof) {
      ch += (Eneg = ch[1]=='-') + (ch[1]=='+');
    }
    ch += 1;

    uint32_t E = 0;
    while (ch < ctx.eof && (digit = static_cast<uint8_t>(*ch - '0')) < 10 ) {
      E = 10*E + digit;
      ch++;
    }
    if (subnormal) {
      if (E == 0 && acc == 0) /* zero */;
      else if (E == 126 && Eneg && acc) /* subnormal */ E = 0;
      else goto fail;
    } else {
      E = Eneg? 127 - E : 127 + E;
      if (E < 1 || E > 254) goto fail;
    }
    ctx.target->uint32 = (neg << 31) | (E << 23) | (acc);
    ctx.ch = ch;
    return;
  }
  if (ch + 2 < ctx.eof && ch[0]=='N' && ch[1]=='a' && ch[2]=='N') {
    ctx.target->uint32 = NA_FLOAT32_I32;
    ctx.ch = ch + 3;
    return;
  }
  if (ch + 7 < ctx.eof && ch[0]=='I' && ch[1]=='n' && ch[2]=='f' && ch[3]=='i' &&
      ch[4]=='n' && ch[5]=='i' && ch[6]=='t' && ch[7]=='y') {
    ctx.target->uint32 = (neg << 31) | INF_FLOAT32_I32;
    ctx.ch = ch + 8;
    return;
  }

  fail:
    ctx.target->uint32 = NA_FLOAT32_I32;
}

REGISTER_PARSER(PT::Float32Hex)
    ->parser(parse_float32_hex)
    ->name("Float32/hex")
    ->code('f')
    ->type(Type::float32())
    ->successors({PT::Float64Hex, PT::Str32});




//------------------------------------------------------------------------------
// Float64
//------------------------------------------------------------------------------

/**
  * Parse "usual" double literals, in the form
  *
  *   [+|-] (NNN|NNN.|.MMM|NNN.MMM) [(E|e) [+|-] EEE]
  *
  * where `NNN`, `MMM`, `EEE` are one or more decimal digits, representing the
  * whole part, fractional part, and the exponent respectively.
  */
static void parse_float64_simple(const ParseContext& ctx) {
  constexpr int MAX_DIGITS = 18;
  const char* ch = ctx.ch;

  bool neg = 0, Eneg = 0;
  long double r;

  if (ch < ctx.eof) {
    ch += (neg = *ch=='-') + (*ch=='+');
  }

  const char* start = ch; // beginning of the number, without the initial sign
  uint_fast64_t acc = 0;  // mantissa NNN.MMM as a single 64-bit integer NNNMMM
  int_fast32_t e = 0;     // the number's exponent. The value being parsed is
                          // equal to acc·10ᵉ
  uint_fast8_t digit;     // temporary variable, holds last scanned digit.

  // Skip leading zeros
  while (ch < ctx.eof && *ch == '0') ch++;

  // Read the first, integer part of the floating number (but no more than
  // MAX_DIGITS digits).
  int_fast32_t sflimit = MAX_DIGITS;
  while (ch < ctx.eof && (digit = static_cast<uint_fast8_t>(*ch - '0')) < 10 && sflimit) {
    acc = 10*acc + digit;
    sflimit--;
    ch++;
  }

  // If maximum allowed number of digits were read, but more are present -- then
  // we will read and discard those extra digits, but only if they are followed
  // by a decimal point (otherwise it's a just big integer, which should be
  // treated as a string instead of losing precision).
  if (ch < ctx.eof && sflimit == 0 && static_cast<uint_fast8_t>(*ch - '0') < 10) {
    while (ch < ctx.eof && static_cast<uint_fast8_t>(*ch - '0') < 10) {
      ch++;
      e++;
    }
    if (ch == ctx.eof || *ch != ctx.dec) goto fail;
  }

  // Read the fractional part of the number, if it's present
  if (ch < ctx.eof && *ch == ctx.dec) {
    ch++;  // skip the dot
    // If the integer part was 0, then leading zeros in the fractional part do
    // not count against the number's precision: skip them.
    if (ch < ctx.eof && *ch == '0' && acc == 0) {
      int_fast32_t k = 0;
      while (ch + k < ctx.eof && ch[k] == '0') k++;
      ch += k;
      e = -k;
    }
    // Now read the significant digits in the fractional part of the number
    int_fast32_t k = 0;
    while (ch + k < ctx.eof && (digit = static_cast<uint_fast8_t>(ch[k] - '0')) < 10 && sflimit) {
      acc = 10*acc + digit;
      k++;
      sflimit--;
    }
    ch += k;
    e -= k;
    // If more digits are present, skip them
    if (ch < ctx.eof && sflimit == 0 && static_cast<uint_fast8_t>(*ch - '0') < 10) {
      ch++;
      while (ch < ctx.eof && static_cast<uint_fast8_t>(*ch - '0') < 10) ch++;
    }
    // Check that at least 1 digit was present either in the integer or
    // fractional part ("+1" here accounts for the decimal point symbol).
    if (ch == start + 1) goto fail;
  }
  // If there is no fractional part, then check that the integer part actually
  // exists (otherwise it's not a valid number)...
  else {
    if (ch == start) goto fail;
  }

  // Now scan the "exponent" part of the number (if present)
  if (ch < ctx.eof && (*ch == 'E' || *ch == 'e')) {

    if (ch + 1 < ctx.eof) {
      ch += (Eneg = (ch[1]=='-')) + (ch[1]=='+');
    }
    ch += 1/*E*/;

    int_fast32_t exp = 0;
    if (ch < ctx.eof && (digit = static_cast<uint_fast8_t>(*ch - '0')) < 10) {
      exp = digit;
      ch++;
      if (ch < ctx.eof && (digit = static_cast<uint_fast8_t>(*ch - '0')) < 10) {
        exp = exp*10 + digit;
        ch++;
        if (ch < ctx.eof && (digit = static_cast<uint_fast8_t>(*ch - '0')) < 10) {
          exp = exp*10 + digit;
          ch++;
        }
      }
    } else {
      goto fail;
    }
    e += Eneg? -exp : exp;
  }

  if (e < -350 || e > 350) goto fail;
  r = static_cast<long double>(acc);

  // Handling of very small and very large floats.
  // Based on https://github.com/Rdatatable/data.table/pull/4165
  if (e < -300 || e > 300) {
    auto extra = static_cast<int_fast8_t>(e - copysign(300, e));
    r *= dt::read::pow10lookup[extra + 300];
    e -= extra;
  }
  e += 300;
  r *= dt::read::pow10lookup[e];
  ctx.target->float64 = static_cast<double>(neg? -r : r);

  ctx.ch = ch;
  return;

  fail: {
    ctx.target->uint64 = NA_FLOAT64_I64;
  }
}

REGISTER_PARSER(PT::Float64Plain)
    ->parser(parse_float64_simple)
    ->name("Float64")
    ->code('F')
    ->type(Type::float64())
    ->successors({PT::Float64Ext, PT::Str32});




//------------------------------------------------------------------------------
// Float64/Ext
//------------------------------------------------------------------------------

/**
  * Parses double values, but also understands various forms of NAN literals
  * (each can possibly be preceded with a `+` or `-` sign):
  *
  *   nan, inf, NaN, NAN, NaN%, NaNQ, NaNS, qNaN, sNaN, NaN12345, sNaN54321,
  *   1.#SNAN, 1.#QNAN, 1.#IND, 1.#INF, INF, Inf, Infinity,
  *   #DIV/0!, #VALUE!, #NULL!, #NAME?, #NUM!, #REF!, #N/A
  *
  */
static void parse_float64_extended(const ParseContext& ctx) {
  const char* ch = ctx.ch;
  uint64_t neg = 0;
  bool quoted = 0;

  if (ch < ctx.eof) {
    ch += (quoted = (*ch==ctx.quote));
    if (ch < ctx.eof) {
      ch += (neg = (*ch=='-')) + (*ch=='+');
    }
  }

  if (ch + 2 < ctx.eof && ch[0]=='n' && ch[1]=='a' && ch[2]=='n' && (ch += 3)) goto return_nan;
  if (ch + 2 < ctx.eof && ch[0]=='i' && ch[1]=='n' && ch[2]=='f' && (ch += 3)) goto return_inf;
  if (ch + 2 < ctx.eof && ch[0]=='I' && ch[1]=='N' && ch[2]=='F' && (ch += 3)) goto return_inf;
  if (ch + 2 < ctx.eof && ch[0]=='I' && ch[1]=='n' && ch[2]=='f' && (ch += 3)) {
    if (ch + 4 < ctx.eof && ch[0]=='i' && ch[1]=='n' && ch[2]=='i' && ch[3]=='t' && ch[4]=='y') ch += 5;
    goto return_inf;
  }
  if (ch + 2 < ctx.eof && ch[0]=='N' && (ch[1]=='A' || ch[1]=='a') && ch[2]=='N' && (ch += 3)) {
    if (ch < ctx.eof && ch[-2]=='a' && (*ch=='%' || *ch=='Q' || *ch=='S')) ch++;
    while (ch < ctx.eof && static_cast<uint_fast8_t>(*ch-'0') < 10) ch++;
    goto return_nan;
  }
  if (ch + 3 < ctx.eof && (ch[0]=='q' || ch[0]=='s') && ch[1]=='N' && ch[2]=='a' && ch[3]=='N' && (ch += 4)) {
    while (ch < ctx.eof && static_cast<uint_fast8_t>(*ch-'0') < 10) ch++;
    goto return_nan;
  }
  if (ch + 2 < ctx.eof && ch[0]=='1' && ch[1]=='.' && ch[2]=='#') {
    if (ch + 6 < ctx.eof && (ch[3]=='S' || ch[3]=='Q') && ch[4]=='N' && ch[5]=='A' && ch[6]=='N' && (ch += 7)) goto return_nan;
    if (ch + 5 < ctx.eof && ch[3]=='I' && ch[4]=='N' && ch[5]=='D' && (ch += 6)) goto return_nan;
    if (ch + 5 < ctx.eof && ch[3]=='I' && ch[4]=='N' && ch[5]=='F' && (ch += 6)) goto return_inf;
  }
  if (ch < ctx.eof && ch[0]=='#') {  // Excel-specific "numbers"
    if (ch + 6 < ctx.eof && ch[1]=='D' && ch[2]=='I' && ch[3]=='V' && ch[4]=='/' && ch[5]=='0' && ch[6]=='!' && (ch += 7)) goto return_nan;
    if (ch + 6 < ctx.eof && ch[1]=='V' && ch[2]=='A' && ch[3]=='L' && ch[4]=='U' && ch[5]=='E' && ch[6]=='!' && (ch += 7)) goto return_nan;
    if (ch + 5 < ctx.eof && ch[1]=='N' && ch[2]=='U' && ch[3]=='L' && ch[4]=='L' && ch[5]=='!' && (ch += 6)) goto return_na;
    if (ch + 5 < ctx.eof && ch[1]=='N' && ch[2]=='A' && ch[3]=='M' && ch[4]=='E' && ch[5]=='?' && (ch += 6)) goto return_na;
    if (ch + 4 < ctx.eof && ch[1]=='N' && ch[2]=='U' && ch[3]=='M' && ch[4]=='!' && (ch += 5)) goto return_na;
    if (ch + 4 < ctx.eof && ch[1]=='R' && ch[2]=='E' && ch[3]=='F' && ch[4]=='!' && (ch += 5)) goto return_na;
    if (ch + 3 < ctx.eof && ch[1]=='N' && ch[2]=='/' && ch[3]=='A' && (ch += 4)) goto return_na;
  }
  parse_float64_simple(ctx);
  return;

  return_inf:
    ctx.target->uint64 = (neg << 63) | INF_FLOAT64_I64;
    goto ok;
  return_nan:
  return_na:
    ctx.target->uint64 = NA_FLOAT64_I64;
  ok:
    if (quoted && ((ch < ctx.eof && *ch!=ctx.quote) || ch == ctx.eof)) {
      ctx.target->uint64 = NA_FLOAT64_I64;
    } else {
      ctx.ch = ch + quoted;
    }
}

REGISTER_PARSER(PT::Float64Ext)
    ->parser(parse_float64_extended)
    ->name("Float64/ext")
    ->code('F')
    ->type(Type::float64())
    ->successors({PT::Str32});




//------------------------------------------------------------------------------
// Float64/Hex
//------------------------------------------------------------------------------

/**
  * Parser for hexadecimal doubles. This format is used in Java (via
  * `Double.toHexString(x)`), in C (`printf("%a", x)`), and in Python
  * (`x.hex()`).
  *
  * The numbers are in the following format:
  *
  *   [+|-] (0x|0X) (0.|1.) HexDigits (p|P) [+|-] DecExponent
  *
  * Thus the number has optional sign; followed by hex prefix `0x` or `0X`;
  * followed by hex significand which may be in the form of either `0.HHHHH...`
  * or `1.HHHHH...` where `H` are hex-digits (there can be no more than 13
  * digits; first form is used for subnormal numbers, second for normal ones);
  * followed by exponent indicator `p` or `P`; followed by optional exponent
  * sign; and lastly followed by the exponent which is a decimal number.
  *
  * This can be directly converted into IEEE-754 double representation:
  *
  *   <1 bit: sign> <11 bits: exp+1022> <52 bits: significand>
  *
  * This parser also recognizes literals "NaN" and "Infinity" which can be
  * produced by Java.
  *
  * @see http://docs.oracle.com/javase/specs/jls/se8/html/jls-3.html#jls-3.10.2
  * @see https://en.wikipedia.org/wiki/IEEE_754-1985
  */
static void parse_float64_hex(const ParseContext& ctx) {
  const char* ch = ctx.ch;
  uint64_t neg = 0;
  uint8_t digit;
  bool Eneg = 0, subnormal = 0;

  if (ch < ctx.eof) {
    ch += (neg = (*ch=='-')) + (*ch=='+');
  }

  if (ch + 2 < ctx.eof && ch[0]=='0' && (ch[1]=='x' || ch[1]=='X') &&
      (ch[2]=='1' || (subnormal = (ch[2]=='0')) != 0)) {
    ch += 3;
    uint64_t acc = 0;
    if (ch < ctx.eof && *ch == '.') {
      ch++;
      int ndigits = 0;
      while (ch < ctx.eof && (digit = dt::read::hexdigits[static_cast<uint8_t>(*ch)]) < 16) {
        acc = (acc << 4) + digit;
        ch++;
        ndigits++;
      }
      if (ndigits > 13) goto fail;
      acc <<= (13 - ndigits) * 4;
    }
    if (ch < ctx.eof && *ch!='p' && *ch!='P') goto fail;


    if (ch + 1 < ctx.eof) {
      ch += (Eneg = (ch[1]=='-')) + (ch[1]=='+');
    }
    ch += 1;

    uint64_t E = 0;
    while (ch < ctx.eof && (digit = static_cast<uint8_t>(*ch - '0')) < 10 ) {
      E = 10*E + digit;
      ch++;
    }
    if (subnormal) {
      if (E == 0 && acc == 0) /* zero */;
      else if (E == 1022 && Eneg && acc) /* subnormal */ E = 0;
      else goto fail;
    } else {
      E = Eneg? 1023 - E : 1023 + E;
      if (E < 1 || E > 2046) goto fail;
    }
    ctx.target->uint64 = (neg << 63) | (E << 52) | (acc);
    ctx.ch = ch;
    return;
  }
  if (ch + 2 < ctx.eof && ch[0]=='N' && ch[1]=='a' && ch[2]=='N') {
    ctx.target->uint64 = NA_FLOAT64_I64;
    ctx.ch = ch + 3;
    return;
  }
  if (ch + 7 < ctx.eof && ch[0]=='I' && ch[1]=='n' && ch[2]=='f' && ch[3]=='i' &&
      ch[4]=='n' && ch[5]=='i' && ch[6]=='t' && ch[7]=='y') {
    ctx.target->uint64 = (neg << 63) | INF_FLOAT64_I64;
    ctx.ch = ch + 8;
    return;
  }

  fail:
    ctx.target->uint64 = NA_FLOAT64_I64;
}

REGISTER_PARSER(PT::Float64Hex)
    ->parser(parse_float64_hex)
    ->name("Float64/hex")
    ->code('F')
    ->type(Type::float64())
    ->successors({PT::Str32});




}}  // namespace dt::read::
