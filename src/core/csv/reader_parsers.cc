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

using namespace dt::read;







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
  autoadd(dt::read::PT::Float32Hex);
  autoadd(dt::read::PT::Float64Plain);
  autoadd(dt::read::PT::Float64Ext);
  autoadd(dt::read::PT::Float64Hex);
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

