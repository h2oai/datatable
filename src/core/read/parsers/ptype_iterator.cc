//------------------------------------------------------------------------------
// Copyright 2018-2021 H2O.ai
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
#include "csv/reader.h"
#include "read/parsers/ptype_iterator.h"
namespace dt {
namespace read {



PTypeIterator::PTypeIterator(PT pt, RT rt, int8_t* qr_ptr)
  : pqr(qr_ptr), rtype(rt), orig_ptype(pt), curr_ptype(pt) {}

PT PTypeIterator::operator*() const {
  return curr_ptype;
}

RT PTypeIterator::get_rtype() const {
  return rtype;
}

PTypeIterator& PTypeIterator::operator++() {
  while (true) {
    if (curr_ptype < PT::Str32) {
      curr_ptype = static_cast<PT>(curr_ptype + 1);
      if (curr_ptype == PT::Date32ISO && !parse_dates) continue;
      if (curr_ptype == PT::Time64ISO && !parse_times) continue;
    } else {
      *pqr = *pqr + 1;
      xassert(*pqr <= 3);
    }
    return *this;
  }
}

bool PTypeIterator::has_incremented() const {
  return curr_ptype != orig_ptype;
}




}}  // namespace dt::read::
