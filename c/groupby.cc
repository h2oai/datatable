//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#include "groupby.h"
#include "utils/exceptions.h"


Groupby::Groupby() : n(0) {}


Groupby::Groupby(size_t _n, MemoryRange&& _offs) {
  if (_offs.size() < sizeof(int32_t) * (_n + 1)) {
    throw RuntimeError() << "Cannot create groupby for " << _n << " groups "
        "from memory buffer of size " << _offs.size();
  }
  if (_offs.get_element<int32_t>(0) != 0) {
    throw RuntimeError() << "Invalid memory buffer for the Groupby: its first "
        "element is not 0.";
  }
  offsets = std::move(_offs);
  n = _n;
}


Groupby Groupby::single_group(int64_t nrows) {
  MemoryRange mr(2 * sizeof(int32_t));
  mr.set_element<int32_t>(0, 0);
  mr.set_element<int32_t>(1, static_cast<int32_t>(nrows));
  size_t n = nrows? 1 : 0;
  return Groupby(n, std::move(mr));
}


const int32_t* Groupby::offsets_r() const {
  return static_cast<const int32_t*>(offsets.rptr());
}


size_t Groupby::ngroups() const {
  return n;
}
