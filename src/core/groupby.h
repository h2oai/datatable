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


/**
  * Groupby object describes how a Frame can be split into groups.
  *
  * This object does not carry a reference to the Frame it is
  * splitting, in fact the same Groupby can be applied to multiple
  * Frames provided they have the same number of rows. This
  * possibility is actually used in EvalContext, where a single
  * Groupby may operate on multiple joined frames.
  *
  * The object contains fields `ngroups_` (which could be 0 or more),
  * and `offsets_`, which is an array of cumulative offsets of each
  * subsequent group. The `offsets_` array has `ngroups_ + 1` elements
  * and the first offset is always 0. The last offset (available via
  * `.last_offset()`) is equal to the number of rows that in a Frame
  * that this Groupby applies to.
  *
  * If the target Frame has 0 rows, then it can be described as either
  * 0 groups, or as 1 group of size 0. In all other cases offsets are
  * strictly increasing: groups of size 0 are not allowed.
  *
  * An empty Groupby can be created using the default constructor:
  * `Groupby()`. Unlike any "normal" Groupby, in this case the
  * `offsets_` array will be empty. Use `operator bool()` to test
  * whether a Groupby object is empty or not.
  *
  */
class Groupby {
  private:
    Buffer offsets_;
    size_t ngroups_;

  public:
    Groupby();
    Groupby(size_t _n, Buffer&& _offs);
    Groupby(const Groupby&) = default;
    Groupby(Groupby&&) = default;
    Groupby& operator=(const Groupby&) = default;
    Groupby& operator=(Groupby&&) = default;
    static Groupby zero_groups();
    static Groupby single_group(size_t nrows);

    explicit operator bool() const;

    size_t size() const noexcept;
    size_t last_offset() const noexcept;
    const int32_t* offsets_r() const;
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
