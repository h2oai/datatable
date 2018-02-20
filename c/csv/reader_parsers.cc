//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#include "csv/reader_parsers.h"



//------------------------------------------------------------------------------
// Boolean
//------------------------------------------------------------------------------

/**
 * "Mu" type is not a boolean -- it's a root for all other types -- however if
 * a column is detected as Mu (i.e. it has no data in it), then we'll return it
 * to the user as a boolean column. This is why we're saving the NA_BOOL8 value
 * here.
 * Note that parsing itself is a noop: Mu type is matched by empty column only,
 * and there is nothing to read nor parsing pointer to advance in an empty
 * column.
 */
void parse_mu(FreadTokenizer& ctx) {
  ctx.target->int8 = NA_BOOL8;
}


/* Parse numbers 0 | 1 as boolean. */
void parse_bool8_numeric(FreadTokenizer& ctx) {
  const char* ch = ctx.ch;
  // *ch=='0' => d=0,
  // *ch=='1' => d=1,
  // *ch==(everything else) => d>1
  uint8_t d = static_cast<uint8_t>(*ch - '0');
  if (d <= 1) {
    ctx.target->int8 = static_cast<int8_t>(d);
    ctx.ch = ch + 1;
  } else {
    ctx.target->int8 = NA_BOOL8;
  }
}


/* Parse lowercase true | false as boolean. */
void parse_bool8_lowercase(FreadTokenizer& ctx) {
  const char* ch = ctx.ch;
  if (ch[0]=='f' && ch[1]=='a' && ch[2]=='l' && ch[3]=='s' && ch[4]=='e') {
    ctx.target->int8 = 0;
    ctx.ch = ch + 5;
  } else if (ch[0]=='t' && ch[1]=='r' && ch[2]=='u' && ch[3]=='e') {
    ctx.target->int8 = 1;
    ctx.ch = ch + 4;
  } else {
    ctx.target->int8 = NA_BOOL8;
  }
}


/* Parse titlecase True | False as boolean. */
void parse_bool8_titlecase(FreadTokenizer& ctx) {
  const char* ch = ctx.ch;
  if (ch[0]=='F' && ch[1]=='a' && ch[2]=='l' && ch[3]=='s' && ch[4]=='e') {
    ctx.target->int8 = 0;
    ctx.ch = ch + 5;
  } else if (ch[0]=='T' && ch[1]=='r' && ch[2]=='u' && ch[3]=='e') {
    ctx.target->int8 = 1;
    ctx.ch = ch + 4;
  } else {
    ctx.target->int8 = NA_BOOL8;
  }
}


/* Parse uppercase TRUE | FALSE as boolean. */
void parse_bool8_uppercase(FreadTokenizer& ctx) {
  const char* ch = ctx.ch;
  if (ch[0]=='F' && ch[1]=='A' && ch[2]=='L' && ch[3]=='S' && ch[4]=='E') {
    ctx.target->int8 = 0;
    ctx.ch = ch + 5;
  } else if (ch[0]=='T' && ch[1]=='R' && ch[2]=='U' && ch[3]=='E') {
    ctx.target->int8 = 1;
    ctx.ch = ch + 4;
  } else {
    ctx.target->int8 = NA_BOOL8;
  }
}



//------------------------------------------------------------------------------
// Int32
//------------------------------------------------------------------------------

// Note: the input buffer must not end with a digit (otherwise buffer overrun
// would occur)!
// See microbench/fread/int32.cpp for performance tests
//
void parse_int32_simple(FreadTokenizer& ctx) {
  const char* ch = ctx.ch;
  bool negative = (*ch == '-');
  ch += (negative || *ch == '+');
  const char* start = ch;  // to check if at least one digit is present
  uint_fast64_t acc = 0;   // value accumulator
  uint8_t  digit;          // current digit being read
  int sf = 0;              // number of significant digits (without initial 0s)

  while (*ch=='0') ch++;   // skip leading zeros
  while ((digit = static_cast<uint8_t>(ch[sf] - '0')) < 10) {
    acc = 10*acc + digit;
    sf++;
  }
  ch += sf;
  // Usually `0 < sf < 10`, and the condition short-circuits.
  // If `sf == 0` then the input is valid iff it is "0" (or multiple 0s,
  // possibly with a sign), which can be checked via `ch > start`.
  // If `sf == 10`, then we explicitly check for overflow ≤2147483647.
  if ((sf? sf < 10 : ch > start) || (sf == 10 && acc <= INT32_MAX)) {
    ctx.target->int32 = negative? -(int32_t)acc : (int32_t)acc;
    ctx.ch = ch;
  } else {
    ctx.target->int32 = NA_INT32;
  }
}



//------------------------------------------------------------------------------
// Int64
//------------------------------------------------------------------------------

void parse_int64_simple(FreadTokenizer& ctx) {
  const char* ch = ctx.ch;
  bool negative = (*ch == '-');
  ch += (negative || *ch == '+');
  const char* start = ch;  // to check if at least one digit is present
  uint_fast64_t acc = 0;   // value accumulator
  uint8_t  digit;          // current digit being read
  int sf = 0;              // number of significant digits (without initial 0s)

  while (*ch=='0') ch++;   // skip leading zeros
  while ((digit = static_cast<uint8_t>(ch[sf] - '0')) < 10) {
    acc = 10*acc + digit;
    sf++;
  }
  ch += sf;
  // The largest admissible value is "9223372036854775807", which has 19 digits.
  // At the same time `uint64_t` can hold values up to 18446744073709551615,
  // which is sufficient to hold any 19-digit values (even 10**19 - 1).
  if ((sf? sf < 19 : ch > start) || (sf == 19 && acc <= INT64_MAX)) {
    ctx.target->int64 = negative? -(int64_t)acc : (int64_t)acc;
    ctx.ch = ch;
  } else {
    ctx.target->int64 = NA_INT64;
  }
}



//------------------------------------------------------------------------------
// Float32
//------------------------------------------------------------------------------

void parse_float32_hex(FreadTokenizer& ctx) {
  const char* ch = ctx.ch;
  uint32_t neg;
  uint8_t digit;
  bool Eneg, subnormal = 0;
  ch += (neg = (*ch=='-')) + (*ch=='+');

  if (ch[0]=='0' && (ch[1]=='x' || ch[1]=='X') &&
      (ch[2]=='1' || (subnormal = ch[2]=='0'))) {
    ch += 3;
    uint32_t acc = 0;
    if (*ch == '.') {
      ch++;
      int ndigits = 0;
      while ((digit = hexdigits[static_cast<uint8_t>(*ch)]) < 16) {
        acc = (acc << 4) + digit;
        ch++;
        ndigits++;
      }
      if (ndigits > 6) goto fail;
      acc <<= 24 - ndigits * 4;
      acc >>= 1;
    }
    if (*ch!='p' && *ch!='P') goto fail;
    ch += 1 + (Eneg = ch[1]=='-') + (ch[1]=='+');
    uint32_t E = 0;
    while ( (digit = static_cast<uint8_t>(*ch - '0')) < 10 ) {
      E = 10*E + digit;
      ch++;
    }
    if (subnormal) {
      if (E == 0 && acc == 0) /* zero */;
      else if (E == 126 && Eneg && acc) /* subnormal */ E = 0;
      else goto fail;
    } else {
      E = 127 + (E ^ -Eneg) + Eneg;
      if (E < 1 || E > 254) goto fail;
    }
    ctx.target->uint32 = (neg << 31) | (E << 23) | (acc);
    ctx.ch = ch;
    return;
  }
  if (ch[0]=='N' && ch[1]=='a' && ch[2]=='N') {
    ctx.target->uint32 = NA_FLOAT32_I32;
    ctx.ch = ch + 3;
    return;
  }
  if (ch[0]=='I' && ch[1]=='n' && ch[2]=='f' && ch[3]=='i' &&
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
void parse_float64_simple(FreadTokenizer& ctx) {
  constexpr int MAX_DIGITS = 18;
  const char* ch = ctx.ch;

  bool neg, Eneg;
  double r;
  ch += (neg = *ch=='-') + (*ch=='+');

  const char* start = ch; // beginning of the number, without the initial sign
  uint_fast64_t acc = 0;  // mantissa NNN.MMM as a single 64-bit integer NNNMMM
  int_fast32_t e = 0;     // the number's exponent. The value being parsed is
                          // equal to acc·10ᵉ
  uint_fast8_t digit;     // temporary variable, holds last scanned digit.

  // Skip leading zeros
  while (*ch == '0') ch++;

  // Read the first, integer part of the floating number (but no more than
  // MAX_DIGITS digits).
  int_fast32_t sflimit = MAX_DIGITS;
  while ((digit = static_cast<uint_fast8_t>(*ch - '0')) < 10 && sflimit) {
    acc = 10*acc + digit;
    sflimit--;
    ch++;
  }

  // If maximum allowed number of digits were read, but more are present -- then
  // we will read and discard those extra digits, but only if they are followed
  // by a decimal point (otherwise it's a just big integer, which should be
  // treated as a string instead of losing precision).
  if (sflimit == 0 && static_cast<uint_fast8_t>(*ch - '0') < 10) {
    while (static_cast<uint_fast8_t>(*ch - '0') < 10) {
      ch++;
      e++;
    }
    if (*ch != ctx.dec) goto fail;
  }

  // Read the fractional part of the number, if it's present
  if (*ch == ctx.dec) {
    ch++;  // skip the dot
    // If the integer part was 0, then leading zeros in the fractional part do
    // not count against the number's precision: skip them.
    if (*ch == '0' && acc == 0) {
      int_fast32_t k = 0;
      while (ch[k] == '0') k++;
      ch += k;
      e = -k;
    }
    // Now read the significant digits in the fractional part of the number
    int_fast32_t k = 0;
    while ((digit = static_cast<uint_fast8_t>(ch[k] - '0')) < 10 && sflimit) {
      acc = 10*acc + digit;
      k++;
      sflimit--;
    }
    ch += k;
    e -= k;
    // If more digits are present, skip them
    if (sflimit == 0 && static_cast<uint_fast8_t>(*ch - '0') < 10) {
      ch++;
      while (static_cast<uint_fast8_t>(*ch - '0') < 10) ch++;
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
  if (*ch == 'E' || *ch == 'e') {
    ch += 1/*E*/ + (Eneg = ch[1]=='-') + (ch[1]=='+');
    int_fast32_t exp = 0;
    if ((digit = static_cast<uint_fast8_t>(*ch - '0')) < 10) {
      exp = digit;
      ch++;
      if ((digit = static_cast<uint_fast8_t>(*ch - '0')) < 10) {
        exp = exp*10 + digit;
        ch++;
        if ((digit = static_cast<uint_fast8_t>(*ch - '0')) < 10) {
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

  r = static_cast<double>(static_cast<long double>(acc) * pow10lookup[e]);
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
void parse_float64_extended(FreadTokenizer& ctx) {
  const char* ch = ctx.ch;
  uint64_t neg;
  bool quoted;
  ch += (quoted = (*ch==ctx.quote));
  ch += (neg = (*ch=='-')) + (*ch=='+');

  if (ch[0]=='n' && ch[1]=='a' && ch[2]=='n' && (ch += 3)) goto return_nan;
  if (ch[0]=='i' && ch[1]=='n' && ch[2]=='f' && (ch += 3)) goto return_inf;
  if (ch[0]=='I' && ch[1]=='N' && ch[2]=='F' && (ch += 3)) goto return_inf;
  if (ch[0]=='I' && ch[1]=='n' && ch[2]=='f' && (ch += 3)) {
    if (ch[0]=='i' && ch[1]=='n' && ch[2]=='i' && ch[3]=='t' && ch[4]=='y') ch += 5;
    goto return_inf;
  }
  if (ch[0]=='N' && (ch[1]=='A' || ch[1]=='a') && ch[2]=='N' && (ch += 3)) {
    if (ch[-2]=='a' && (*ch=='%' || *ch=='Q' || *ch=='S')) ch++;
    while ((uint_fast8_t)(*ch-'0') < 10) ch++;
    goto return_nan;
  }
  if ((ch[0]=='q' || ch[0]=='s') && ch[1]=='N' && ch[2]=='a' && ch[3]=='N' && (ch += 4)) {
    while ((uint_fast8_t)(*ch-'0') < 10) ch++;
    goto return_nan;
  }
  if (ch[0]=='1' && ch[1]=='.' && ch[2]=='#') {
    if ((ch[3]=='S' || ch[3]=='Q') && ch[4]=='N' && ch[5]=='A' && ch[6]=='N' && (ch += 7)) goto return_nan;
    if (ch[3]=='I' && ch[4]=='N' && ch[5]=='D' && (ch += 6)) goto return_nan;
    if (ch[3]=='I' && ch[4]=='N' && ch[5]=='F' && (ch += 6)) goto return_inf;
  }
  if (ch[0]=='#') {  // Excel-specific "numbers"
    if (ch[1]=='D' && ch[2]=='I' && ch[3]=='V' && ch[4]=='/' && ch[5]=='0' && ch[6]=='!' && (ch += 7)) goto return_nan;
    if (ch[1]=='V' && ch[2]=='A' && ch[3]=='L' && ch[4]=='U' && ch[5]=='E' && ch[6]=='!' && (ch += 7)) goto return_nan;
    if (ch[1]=='N' && ch[2]=='U' && ch[3]=='L' && ch[4]=='L' && ch[5]=='!' && (ch += 6)) goto return_na;
    if (ch[1]=='N' && ch[2]=='A' && ch[3]=='M' && ch[4]=='E' && ch[5]=='?' && (ch += 6)) goto return_na;
    if (ch[1]=='N' && ch[2]=='U' && ch[3]=='M' && ch[4]=='!' && (ch += 5)) goto return_na;
    if (ch[1]=='R' && ch[2]=='E' && ch[3]=='F' && ch[4]=='!' && (ch += 5)) goto return_na;
    if (ch[1]=='N' && ch[2]=='/' && ch[3]=='A' && (ch += 4)) goto return_na;
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
    if (quoted && *ch!=ctx.quote) {
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
void parse_float64_hex(FreadTokenizer& ctx) {
  const char* ch = ctx.ch;
  uint64_t neg;
  uint8_t digit;
  bool Eneg, subnormal = 0;
  ch += (neg = (*ch=='-')) + (*ch=='+');

  if (ch[0]=='0' && (ch[1]=='x' || ch[1]=='X') && (ch[2]=='1' || (subnormal = ch[2]=='0'))) {
    ch += 3;
    uint64_t acc = 0;
    if (*ch == '.') {
      ch++;
      int ndigits = 0;
      while ((digit = hexdigits[static_cast<uint8_t>(*ch)]) < 16) {
        acc = (acc << 4) + digit;
        ch++;
        ndigits++;
      }
      if (ndigits > 13) goto fail;
      acc <<= (13 - ndigits) * 4;
    }
    if (*ch!='p' && *ch!='P') goto fail;
    ch += 1 + (Eneg = ch[1]=='-') + (ch[1]=='+');
    uint64_t E = 0;
    while ( (digit = static_cast<uint8_t>(*ch - '0')) < 10 ) {
      E = 10*E + digit;
      ch++;
    }
    if (subnormal) {
      if (E == 0 && acc == 0) /* zero */;
      else if (E == 1022 && Eneg && acc) /* subnormal */ E = 0;
      else goto fail;
    } else {
      E = 1023 + (E ^ -Eneg) + Eneg;
      if (E < 1 || E > 2046) goto fail;
    }
    ctx.target->uint64 = (neg << 63) | (E << 52) | (acc);
    ctx.ch = ch;
    return;
  }
  if (ch[0]=='N' && ch[1]=='a' && ch[2]=='N') {
    ctx.target->uint64 = NA_FLOAT64_I64;
    ctx.ch = ch + 3;
    return;
  }
  if (ch[0]=='I' && ch[1]=='n' && ch[2]=='f' && ch[3]=='i' &&
      ch[4]=='n' && ch[5]=='i' && ch[6]=='t' && ch[7]=='y') {
    ctx.target->uint64 = (neg << 63) | INF_FLOAT64_I64;
    ctx.ch = ch + 8;
    return;
  }

  fail:
    ctx.target->uint64 = NA_FLOAT64_I64;
}



//------------------------------------------------------------------------------
// String
//------------------------------------------------------------------------------

// TODO: refactor into smaller pieces
void parse_string(FreadTokenizer& ctx) {
  const char* ch = ctx.ch;
  const char quote = ctx.quote;
  const char sep = ctx.sep;

  // need to skip_white first for the reason that a quoted field might have space before the
  // quote; e.g. test 1609. We need to skip the space(s) to then switch on quote or not.
  if (*ch==' ' && ctx.stripWhite) while(*++ch==' ');  // if sep==' ' the space would have been skipped already and we wouldn't be on space now.
  const char* fieldStart = ch;
  if (*ch!=quote || ctx.quoteRule==3) {
    // Most common case: unambiguously not quoted. Simply search for sep|eol.
    // If field contains sep|eol then it should have been quoted and we do not
    // try to heal that.
    while (1) {
      if (*ch == sep) break;
      if (static_cast<uint8_t>(*ch) <= 13) {
        if (*ch == '\0' || *ch == '\n') break;
        if (*ch == '\r') {
          if (!ctx.LFpresent || ch[1] == '\n') break;
          const char *tch = ch + 1;
          while (*tch == '\r') tch++;
          if (*tch == '\n') break;
        }
      }
      ch++;  // sep, \r, \n or \0 will end
    }
    ctx.ch = ch;
    int fieldLen = (int)(ch-fieldStart);
    if (ctx.stripWhite) {   // TODO:  do this if and the next one together once in bulk afterwards before push
      while(fieldLen>0 && ch[-1]==' ') { fieldLen--; ch--; }
      // this space can't be sep otherwise it would have stopped the field earlier inside end_of_field()
    }
    if (fieldLen ? ctx.end_NA_string(fieldStart)==ch : ctx.blank_is_a_NAstring) {
      // TODO - speed up by avoiding end_NA_string when there are none
      ctx.target->str32.setna();
    } else {
      ctx.target->str32.offset = (int32_t)(fieldStart - ctx.anchor);
      ctx.target->str32.length = fieldLen;
    }
    return;
  }
  // else *ch==quote (we don't mind that quoted fields are a little slower e.g. no desire to save switch)
  //    the field is quoted and quotes are correctly escaped (quoteRule 0 and 1)
  // or the field is quoted but quotes are not escaped (quoteRule 2)
  // or the field is not quoted but the data contains a quote at the start (quoteRule 2 too)
  int eolCount = 0;
  fieldStart++;  // step over opening quote
  switch(ctx.quoteRule) {
  case 0:  // quoted with embedded quotes doubled; the final unescaped " must be followed by sep|eol
    while (*++ch) {
      if (*ch=='\n' && ++eolCount==100) return;  // TODO: expose this 100 to user to allow them to control limiting runaway fields
      if (*ch==quote) {
        if (ch[1]==quote) { ch++; continue; }
        break;  // found undoubled closing quote
      }
    }
    break;
  case 1:  // quoted with embedded quotes escaped; the final unescaped " must be followed by sep|eol
    while (*++ch) {
      if (*ch=='\n' && ++eolCount==100) return;
      if (*ch=='\\' && (ch[1]==quote || ch[1]=='\\')) { ch++; continue; }
      if (*ch==quote) break;
    }
    break;
  case 2:
    // (i) quoted (perhaps because the source system knows sep is present) but any quotes were not escaped at all,
    // so look for ", to define the end.   (There might not be any quotes present to worry about, anyway).
    // (ii) not-quoted but there is a quote at the beginning so it should have been; look for , at the end
    // If no eol are present inside quoted fields (i.e. rows are simple rows), then this should work ok e.g. test 1453
    // since we look for ", and the source system quoted when , is present, looking for ", should work well.
    // Under this rule, no eol may occur inside fields.
    {
      const char* ch2 = ch;
      while (*++ch && *ch!='\n' && *ch!='\r') {
        if (*ch==quote && (ch[1]==sep || ch[1]=='\r' || ch[1]=='\n')) {
          // (*1) regular ", ending; leave *ch on closing quote
          ch2 = ch;
          break;
        }
        if (*ch==sep) {
          // first sep in this field
          // if there is a ", afterwards but before the next \n, use that; the field was quoted and it's still case (i) above.
          // Otherwise break here at this first sep as it's case (ii) above (the data contains a quote at the start and no sep)
          ch2 = ch;
          while (*++ch2 && *ch2!='\n' && *ch2!='\r') {
            if (*ch2==quote && (ch2[1]==sep || ch2[1]=='\r' || ch2[1]=='\n')) {
              // (*2) move on to that first ", -- that's this field's ending
              ch = ch2;
              break;
            }
          }
          break;
        }
      }
      if (ch!=ch2) fieldStart--;   // field ending is this sep|eol; neither (*1) or (*2) happened; opening quote wasn't really an opening quote
    }
    break;
  default:
    return;  // Internal error: undefined quote rule
  }
  ctx.target->str32.length = (int32_t)(ch - fieldStart);
  ctx.target->str32.offset = (int32_t)(fieldStart - ctx.anchor);
  if (*ch==quote) {
    ctx.ch = ch + 1;
    ctx.skip_white();
  } else {
    ctx.ch = ch;
    if (*ch=='\0') {
      if (ctx.quoteRule!=2) {  // see test 1324 where final field has open quote but not ending quote; include the open quote like quote rule 2
        ctx.target->str32.offset--;
        ctx.target->str32.length++;
      }
    }
    if (ctx.stripWhite) {  // see test 1551.6; trailing whitespace in field [67,V37] == "\"\"A\"\" ST       "
      while (ctx.target->str32.length>0 && ch[-1]==' ') {
        ctx.target->str32.length--;
        ch--;
      }
    }
  }
}



//------------------------------------------------------------------------------
// Other
//------------------------------------------------------------------------------

/*
class PTypeIterator {
  class PTypeIteratorImpl {
    const PT ptype;
    const RT rtype;
    int ipt, irt;
    PTypeIteratorImpl() : ipt(-1) {}
    PTypeIteratorImpl(PT pt, RT rt) : ptype(pt), rtype(rt), ipt(0), irt(0) {}

    PTypeIteratorImpl& operator++() {
      if (ipt >= 0) {
        ipt++;
        if (ipt + ptype == 12) ipt = -1;
      }
      return *this;
    }
    PT operator*() const {
      return ipt >= 0? ptype + ipt : 0;
    }
    bool operator!=(const PTypeIteratorImpl& other) const {
      return ipt != other.ipt;
    }
  };

  const PT ptype;
  const RT rtype;
  PTypeIterator(PT pt, RT rt) : ptype(pt), rtype(rt) {}
  PTypeIteratorImpl begin() const { return PTypeIteratorImpl(ptype, rtype); }
  PTypeIteratorImpl end() const { return PTypeIteratorImpl(); }
};


class CsvReadColumn {
  std::string name;
  PT ptype;
  RT rtype;

  PTypeIterator successor_types() const;
};
*/

ParserLibrary& ParserLibrary::get() {
  static ParserLibrary& instance = *new ParserLibrary();
  return instance;
}

ParserLibrary::ParserLibrary() {
  add(ParserInfo(PT::Drop, "Drop", '-', nullptr));
  add(ParserInfo(PT::Mu, "Mu", '?', parse_mu));
  // {parse_bool8_lowercase, parse_bool8_titlecase, parse_bool8_uppercase, parse_bool8_numeric,
  //  parse_int32_simple, parse_int64_simple};
}

void ParserLibrary::add(ParserInfo&& p) {
  size_t iid = static_cast<size_t>(p.id);
  parsers.reserve(iid + 1);
  parsers[iid] = p;
}
