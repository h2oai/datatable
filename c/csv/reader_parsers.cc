//------------------------------------------------------------------------------
//  Copyright 2017 H2O.ai
//
//  Licensed under the Apache License, Version 2.0 (the "License");
//  you may not use this file except in compliance with the License.
//  You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
//  Unless required by applicable law or agreed to in writing, software
//  distributed under the License is distributed on an "AS IS" BASIS,
//  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//  See the License for the specific language governing permissions and
//  limitations under the License.
//------------------------------------------------------------------------------
#include "csv/reader_parsers.h"
#include <limits>

static const int8_t  NA_BOOL8 = -128;
static const int32_t NA_INT32 = std::numeric_limits<int32_t>::min();
static const int64_t NA_INT64 = std::numeric_limits<int64_t>::min();



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
void parser_Mu(FieldParseContext& ctx) {
  ctx.target->int8 = NA_BOOL8;
}


/* Parse numbers 0 | 1 as boolean. */
void parser_Bool01(FieldParseContext& ctx) {
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
void parser_BoolL(FieldParseContext& ctx) {
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
void parser_BoolT(FieldParseContext& ctx) {
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
void parser_BoolU(FieldParseContext& ctx) {
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
void parser_Int32Plain(FieldParseContext& ctx) {
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

void parser_Int64Plain(FieldParseContext& ctx) {
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
  add(ParserInfo(PT::Mu, "Mu", '?', parser_Mu));
  // {parser_BoolL, parser_BoolT, parser_BoolU, parser_Bool01,
  //  parser_Int32Plain, parser_Int64Plain};
}

void ParserLibrary::add(ParserInfo&& p) {
  size_t iid = static_cast<size_t>(p.id);
  parsers.reserve(iid + 1);
  parsers[iid] = p;
}
