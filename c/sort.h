//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#ifndef dt_SORT_h
#define dt_SORT_h
#include "utils/array.h"  // arr32_t


struct radix_range {
  size_t size;
  size_t offset;
};



/**
 * Helper class to collect grouping information while sorting.
 *
 * The end product of this class is the array of cumulative group sizes. This
 * array will have 1 + ngroups elements, with the first element being 0, and
 * the last the total number of elements in the data being sorted/grouped.
 *
 * In order to accommodate parallel sorting, the array of group sizes is
 * provided externally, and is not managed by this class (only written to).
 *
 * Interface
 * ---------
 * init(groups, cumsize)
 *     Initialize the `groups` pointer and initial `cumsize` value.
 *
 * push(grp)
 *     Add a single group of size `grp`.
 *
 * from_data(data, indices, n)
 *     Given sorted data in the form `[data[indices[i]] for i in range(n)]`,
 *     extract and add group information from it. The groups are detected based
 *     on whether the consecutive elements compare equal or not.
 *
 * from_data(strdata, offsets, strstart, indices, n)
 *     Similar to the previous function, but works with string data.
 *
 * from_chunks(rrmap, nr)
 *     Gather groups information from distinct chunks given by the `rrmap`.
 *     Specifically, for each of the `nr` entries in the `rrmap` array, there
 *     is a chunk of grouping data of size `rrmap[i].size` stored at the offset
 *     `rrmap[i].offset` from the `groups` pointer (the offset is in elements,
 *     not in bytes). Thereby, the purpose of this method is to collect this
 *     information into a contiguous array.
 *
 * from_histogram(histogram, nchunks, nradixes)
 *     Fill the grouping information from the data histogram, as described
 *     in the documentation for `SortContext` class.
 *
 * Internal parameters
 * -------------------
 * groups
 *     The array of cumulative group sizes. The array must be pre-allocated
 *     and passed to this class via `init()`.
 *
 * count
 *     The number of groups that were stored in the `groups` array.
 *
 * cumsize
 *     The total size of all groups added so far. This is always equals to
 *     `groups[count - 1]`.
 *
 */
// TODO: Add support for 64-bit groups
class GroupGatherer {
  private:
    int32_t* groups;  // externally owned pointer
    int32_t  count;
    int32_t  cumsize;

  public:
    GroupGatherer();
    void init(int32_t* data, int32_t cumsize0);

    int32_t* data() const { return groups; }
    size_t   size() const { return static_cast<size_t>(count); }
    int32_t  cumulative_size() const { return cumsize; }
    operator bool() const { return !!groups; }

    void push(size_t grp);

    template <typename T, typename V>
    void from_data(const T*, V*, size_t);

    template <typename T, typename V>
    void from_data(const uint8_t*, const T*, T, V*, size_t, bool descending);

    void from_chunks(radix_range* rrmap, size_t nradixes);

    void from_histogram(size_t* histogram, size_t nchunks, size_t nradixes);
};



//------------------------------------------------------------------------------
// Templates
//------------------------------------------------------------------------------

template <typename T, typename V>
void insert_sort_keys(const T* x, V* o, V* oo, int n, GroupGatherer& gg);

template <typename T, typename V>
void insert_sort_values(const T* x, V* o, int n, GroupGatherer& gg);

template <typename T, typename V>
void insert_sort_keys_str(const uint8_t*, const T*, T, V*, V*, int, GroupGatherer&, bool);

template <typename T, typename V>
void insert_sort_values_str(const uint8_t*, const T*, T, V*, int, GroupGatherer&, bool);

template <int R, typename T>
int compare_offstrings(const uint8_t*, T, T, T, T);



extern template void insert_sort_keys(const uint8_t*,  int32_t*, int32_t*, int, GroupGatherer&);
extern template void insert_sort_keys(const uint16_t*, int32_t*, int32_t*, int, GroupGatherer&);
extern template void insert_sort_keys(const uint32_t*, int32_t*, int32_t*, int, GroupGatherer&);
extern template void insert_sort_keys(const uint64_t*, int32_t*, int32_t*, int, GroupGatherer&);

extern template void insert_sort_values(const uint8_t*,  int32_t*, int, GroupGatherer&);
extern template void insert_sort_values(const uint16_t*, int32_t*, int, GroupGatherer&);
extern template void insert_sort_values(const uint32_t*, int32_t*, int, GroupGatherer&);
extern template void insert_sort_values(const uint64_t*, int32_t*, int, GroupGatherer&);

extern template void insert_sort_keys_str(  const uint8_t*, const uint32_t*, uint32_t, int32_t*, int32_t*, int, GroupGatherer&, bool);
extern template void insert_sort_values_str(const uint8_t*, const uint32_t*, uint32_t, int32_t*, int, GroupGatherer&, bool);
extern template void insert_sort_keys_str(  const uint8_t*, const uint64_t*, uint64_t, int32_t*, int32_t*, int, GroupGatherer&, bool);
extern template void insert_sort_values_str(const uint8_t*, const uint64_t*, uint64_t, int32_t*, int, GroupGatherer&, bool);

extern template int compare_offstrings<1>(const uint8_t*, uint32_t, uint32_t, uint32_t, uint32_t);
extern template int compare_offstrings<1>(const uint8_t*, uint64_t, uint64_t, uint64_t, uint64_t);
extern template int compare_offstrings<-1>(const uint8_t*, uint32_t, uint32_t, uint32_t, uint32_t);
extern template int compare_offstrings<-1>(const uint8_t*, uint64_t, uint64_t, uint64_t, uint64_t);

extern template void GroupGatherer::from_data(const uint8_t*,  int32_t*, size_t);
extern template void GroupGatherer::from_data(const uint16_t*, int32_t*, size_t);
extern template void GroupGatherer::from_data(const uint32_t*, int32_t*, size_t);
extern template void GroupGatherer::from_data(const uint64_t*, int32_t*, size_t);
extern template void GroupGatherer::from_data(const uint8_t*, const uint32_t*, uint32_t, int32_t*, size_t, bool);
extern template void GroupGatherer::from_data(const uint8_t*, const uint64_t*, uint64_t, int32_t*, size_t, bool);


#endif
