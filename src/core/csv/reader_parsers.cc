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
namespace dt {
namespace read {


//------------------------------------------------------------------------------
// PtypeIterator
//------------------------------------------------------------------------------


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
    if (ipt + ptype == dt::read::PT::COUNT) ipt = -1;
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

