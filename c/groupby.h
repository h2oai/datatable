//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#ifndef dt_GROUPBY_h
#define dt_GROUPBY_h
#include "buffer.h"
#include "rowindex.h"


class Groupby {
  private:
    Buffer offsets;
    size_t n;

  public:
    Groupby();
    Groupby(size_t _n, Buffer&& _offs);
    Groupby(const Groupby&) = default;
    Groupby(Groupby&&) = default;
    Groupby& operator=(const Groupby&) = default;
    Groupby& operator=(Groupby&&) = default;
    static Groupby single_group(size_t nrows);

    const int32_t* offsets_r() const;
    size_t ngroups() const;
    size_t size() const noexcept;  // same as ngroups()
    size_t last_offset() const noexcept;
    explicit operator bool() const;
    void get_group(size_t i, size_t* i0, size_t* i1) const;

    // Return a RowIndex which can be used to perform "ungrouping" operation.
    // More specifically, it is a RowIndex with the following data:
    //     [0, 0, ..., 0, 1, 1, ..., 1, 2, ..., n, n, ..., n]
    // Where the first 1 is located at offset `offsets[1]`, the first 2 at the
    // offset `offsets[2]`, and so on, and the total length of the RowIndex is
    // equal to `offsets[n]`.
    //
    // This RowIndex will not be cached within the Groupby, we recommend the
    // caller to do that.
    //
    RowIndex ungroup_rowindex();
};


#endif
