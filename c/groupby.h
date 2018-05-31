//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#ifndef dt_GROUPBY_h
#define dt_GROUPBY_h
#include "memrange.h"


class Groupby {
  private:
    MemoryRange offsets;
    size_t n;

  public:
    Groupby();
    Groupby(size_t _n, MemoryRange&& _offs);
    Groupby(const Groupby&) = default;
    Groupby(Groupby&&) = default;
    Groupby& operator=(const Groupby&) = default;
    Groupby& operator=(Groupby&&) = default;

    const int32_t* offsets_r() const;
    size_t ngroups() const;
};


#endif
