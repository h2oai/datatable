//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#include "groupby.h"


Groupby::Groupby() : n(0) {}


Groupby::Groupby(size_t _n, MemoryRange&& _offs)
  : offsets(std::move(_offs)), n(_n) {}


const int32_t* Groupby::offsets_r() const {
  return static_cast<const int32_t*>(offsets.rptr());
}


size_t Groupby::ngroups() const {
  return n;
}
