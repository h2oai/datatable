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
#include "read/parse_context.h"          // ParseContext
#include "read/field64.h"                // field64
#include "read/constants.h"              // hexdigits, pow10lookup
#include "utils/assert.h"                // xassert
#include "utils/macros.h"
#include "stype.h"


using namespace dt::read;











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
  autoadd(dt::read::PT::Date32ISO);
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

