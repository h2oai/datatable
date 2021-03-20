//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018-2021
//------------------------------------------------------------------------------
#include <iostream>
#include <limits>                        // std::numeric_limits
#include "csv/reader_parsers.h"
#include "lib/hh/date.h"
#include "read/parse_context.h"          // ParseContext
#include "read/field64.h"                // field64
#include "read/constants.h"              // hexdigits, pow10lookup
#include "utils/assert.h"                // xassert
#include "utils/macros.h"
#include "stype.h"

static constexpr int32_t  NA_INT32 = INT32_MIN;
static constexpr uint32_t NA_FLOAT32_I32 = 0x7F8007A2;
static constexpr uint64_t NA_FLOAT64_I64 = 0x7FF000000000DEADull;
static constexpr uint32_t INF_FLOAT32_I32 = 0x7F800000;
static constexpr uint64_t INF_FLOAT64_I64 = 0x7FF0000000000000ull;

using namespace dt::read;





//------------------------------------------------------------------------------
// Float32
//------------------------------------------------------------------------------

void parse_float32_hex(const ParseContext& ctx) {
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
void parse_float64_simple(const ParseContext& ctx) {
  constexpr int MAX_DIGITS = 18;
  const char* ch = ctx.ch;

  bool neg = 0, Eneg = 0;
  double r;

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
  e += 350; // lookup table is arranged from -350 (0) to +350 (700)
  if (e < 0 || e > 700) goto fail;

  r = static_cast<double>(static_cast<long double>(acc) * dt::read::pow10lookup[e]);
  ctx.target->float64 = neg? -r : r;
  ctx.ch = ch;
  return;

  fail: {
    ctx.target->uint64 = NA_FLOAT64_I64;
  }
}



/**
 * Parses double values, but also understands various forms of NAN literals
 * (each can possibly be preceded with a `+` or `-` sign):
 *
 *   nan, inf, NaN, NAN, NaN%, NaNQ, NaNS, qNaN, sNaN, NaN12345, sNaN54321,
 *   1.#SNAN, 1.#QNAN, 1.#IND, 1.#INF, INF, Inf, Infinity,
 *   #DIV/0!, #VALUE!, #NULL!, #NAME?, #NUM!, #REF!, #N/A
 *
 */
void parse_float64_extended(const ParseContext& ctx) {
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
void parse_float64_hex(const ParseContext& ctx) {
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



//------------------------------------------------------------------------------
// Date32
//------------------------------------------------------------------------------

static bool parse_year(const char** pch, const char* eof, int32_t* out) {
  const char* ch = *pch;
  if (ch == eof) return false;
  bool neg = (*ch == '-');
  ch += neg;
  if (ch + 7 < eof) eof = ch + 7;  // year can have up to 7 digits
  const char* ch0 = ch;
  int32_t value = 0;
  uint8_t digit;
  while (ch < eof && (digit = static_cast<uint8_t>(*ch - '0')) < 10) {
    value = value * 10 + static_cast<int32_t>(digit);
    ch++;
  }
  if (ch0 == ch) return false;
  if (neg) value = -value;
  *out = value;
  *pch = ch;
  return true;
}

static bool parse_2digits(const char** pch, const char* eof, int32_t* out) {
  const char* ch = *pch;
  if (ch + 2 >= eof) return false;
  uint8_t digit0 = static_cast<uint8_t>(ch[0] - '0');
  uint8_t digit1 = static_cast<uint8_t>(ch[1] - '0');
  if (digit0 < 10 && digit1 < 10) {
    *out = static_cast<int32_t>(digit0) * 10 + static_cast<int32_t>(digit1);
    *pch = ch + 2;
    return true;
  }
  return false;
}

void parse_date32_iso(const ParseContext& ctx) {
  const char* ch = ctx.ch;
  int32_t year, month, day;
  if (!parse_year(&ch, ctx.eof, &year)) goto fail;
  if (!(ch < ctx.eof && *ch == '-')) goto fail;
  ch++;
  if (!parse_2digits(&ch, ctx.eof, &month)) goto fail;
  if (!(ch < ctx.eof && *ch == '-')) goto fail;
  ch++;
  if (!parse_2digits(&ch, ctx.eof, &day)) goto fail;
  if (!(year >= -5877641 && year <= 5879610)) goto fail;
  if (!(month >= 1 && month <= 12)) goto fail;
  if (!(day >= 1 && day <= hh::last_day_of_month(year, month))) goto fail;
  ctx.target->int32 = hh::days_from_civil(year, month, day);
  ctx.ch = ch;
  return;

  fail:
    ctx.target->int32 = NA_INT32;
}



//------------------------------------------------------------------------------
// ParserLibrary
//------------------------------------------------------------------------------

// The only reason we don't do std::vector<ParserInfo> here is because it causes
// annoying warnings about exit-time / global destructors.
ParserInfo* ParserLibrary::parsers = nullptr;
ParserFnPtr* ParserLibrary::parser_fns = nullptr;

void ParserLibrary::init_parsers() {
  parsers = new ParserInfo[num_parsers];
  parser_fns = new ParserFnPtr[num_parsers];

  auto add = [&](dt::read::PT pt, const char* name, char code, int8_t sz,
                  dt::SType st,ParserFnPtr ptr) {
    size_t iid = static_cast<size_t>(pt);
    xassert(iid < ParserLibrary::num_parsers);
    parsers[iid] = ParserInfo(pt, name, code, sz, st, ptr);
    parser_fns[iid] = ptr;
  };
  auto autoadd = [&](dt::read::PT pt) {
    auto info = ParserLibrary2::all_parsers()[pt];
    xassert(info);
    auto stype = info->type().stype();
    add(pt, info->name().data(), info->code(),
        static_cast<int8_t>(stype_elemsize(stype)), stype, info->parser());
  };

  autoadd(dt::read::PT::Void);
  autoadd(dt::read::PT::Bool01);
  autoadd(dt::read::PT::BoolL);
  autoadd(dt::read::PT::BoolT);
  autoadd(dt::read::PT::BoolU);
  autoadd(dt::read::PT::Int32);
  autoadd(dt::read::PT::Int32Sep);
  autoadd(dt::read::PT::Int64);
  autoadd(dt::read::PT::Int64Sep);
  add(dt::read::PT::Float32Hex,   "Float32/hex",     'f', 4, dt::SType::FLOAT32, parse_float32_hex);
  add(dt::read::PT::Float64Plain, "Float64",         'F', 8, dt::SType::FLOAT64, parse_float64_simple);
  add(dt::read::PT::Float64Ext,   "Float64/ext",     'F', 8, dt::SType::FLOAT64, parse_float64_extended);
  add(dt::read::PT::Float64Hex,   "Float64/hex",     'F', 8, dt::SType::FLOAT64, parse_float64_hex);
  add(dt::read::PT::Date32ISO,    "Date32/iso",      'D', 4, dt::SType::DATE32,  parse_date32_iso);
  add(dt::read::PT::Str32,        "Str32",           's', 4, dt::SType::STR32,   dt::read::parse_string);
  add(dt::read::PT::Str64,        "Str64",           'S', 8, dt::SType::STR64,   dt::read::parse_string);
}


ParserLibrary::ParserLibrary() {
  if (!parsers) init_parsers();
}



//------------------------------------------------------------------------------
// PtypeIterator
//------------------------------------------------------------------------------
namespace dt {
namespace read {


PtypeIterator::PtypeIterator(PT pt, RT rt, int8_t* qr_ptr)
  : pqr(qr_ptr), rtype(rt), orig_ptype(pt), curr_ptype(pt) {}

PT PtypeIterator::operator*() const {
  return curr_ptype;
}

RT PtypeIterator::get_rtype() const {
  return rtype;
}

PtypeIterator& PtypeIterator::operator++() {
  if (curr_ptype < PT::Str32) {
    curr_ptype = static_cast<PT>(curr_ptype + 1);
  } else {
    *pqr = *pqr + 1;
  }
  return *this;
}

bool PtypeIterator::has_incremented() const {
  return curr_ptype != orig_ptype;
}




}}
//------------------------------------------------------------------------------
// ParserIterator
//------------------------------------------------------------------------------

ParserIterable ParserLibrary::successor_types(dt::read::PT pt) const {
  return ParserIterable(pt);
}


ParserIterable::ParserIterable(dt::read::PT pt)
  : ptype(pt) {}

ParserIterator ParserIterable::begin() const {
  return ParserIterator(ptype);
}

ParserIterator ParserIterable::end() const {
  return ParserIterator();
}


ParserIterator::ParserIterator()
  : ipt(-1), ptype(0) {}

ParserIterator::ParserIterator(dt::read::PT pt)
  : ipt(0), ptype(static_cast<uint8_t>(pt))
{
  ++*this;
}


ParserIterator& ParserIterator::operator++() {
  if (ipt >= 0) {
    ipt++;
    if (ipt + ptype == ParserLibrary::num_parsers) ipt = -1;
  }
  return *this;
}

bool ParserIterator::operator==(const ParserIterator& rhs) const {
  return (ipt == -1 && rhs.ipt == -1) ||
         (ipt == rhs.ipt && ptype == rhs.ptype);
}

bool ParserIterator::operator!=(const ParserIterator& rhs) const {
  return (ipt != -1 || rhs.ipt != -1) &&
         (ipt != rhs.ipt || ptype != rhs.ptype);
}

dt::read::PT ParserIterator::operator*() const {
  return static_cast<dt::read::PT>(ptype + ipt);
}

