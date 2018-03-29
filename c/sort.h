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
 * Helper class to collect grouping information while sorting. It has stack-like
 * interface, i.e. the code is expected to simply push sizes of each group as
 * they are encountered.
 *
 * Note that this class builds group information in the form of the cumulative
 * group sizes. The final array it produces has 1 + ngroups elements, with the
 * first element being 0, and the last the total number of elements in the data
 * being sorted/grouped.
 *


 TODO: update


 * Interface
 * ---------
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
 * from_groups(gg)
 *     Adds all groups from another GroupGatherer instance `gg`.
 *
 * Internal parameters
 * -------------------
 * groups
 *     The array of cumulative group sizes. This array must be allocated for at
 *     least `1 + ngroups` elements.
 *
 * count
 *     The count of elements in `groups` that are already in use. Thus, `count`
 *     is 1 + the number of groups.
 *
 * cumsize
 *     The total size of all groups added so far. This is always equals to
 *     `groups[count - 1]`.
 *
 */
class GroupGatherer {
  private:
    int32_t* groups;  // externally owned pointer
    int32_t  count;
    int32_t  cumsize;

  public:
    GroupGatherer();
    void set_ptr(int32_t* data, int32_t cumsize0);

    int32_t* data() const { return groups; }
    int32_t  size() const { return count; }
    int32_t  cumulative_size() const { return cumsize; }
    operator bool() const { return !!groups; }

    void push(size_t grp);
    template <typename T, typename V> void from_data(const T*, V*, size_t);
    template <typename T, typename V> void from_data(const uint8_t*, const T*, T, V*, size_t);
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
void insert_sort_keys_str(const uint8_t*, const T*, T, V*, V*, int, GroupGatherer&);

template <typename T, typename V>
void insert_sort_values_str(const uint8_t*, const T*, T, V*, int, GroupGatherer&);

template <typename T>
int compare_offstrings(const uint8_t*, T, T, T, T);



extern template void insert_sort_keys(const uint8_t*,  int32_t*, int32_t*, int, GroupGatherer&);
extern template void insert_sort_keys(const uint16_t*, int32_t*, int32_t*, int, GroupGatherer&);
extern template void insert_sort_keys(const uint32_t*, int32_t*, int32_t*, int, GroupGatherer&);
extern template void insert_sort_keys(const uint64_t*, int32_t*, int32_t*, int, GroupGatherer&);

extern template void insert_sort_values(const uint8_t*,  int32_t*, int, GroupGatherer&);
extern template void insert_sort_values(const uint16_t*, int32_t*, int, GroupGatherer&);
extern template void insert_sort_values(const uint32_t*, int32_t*, int, GroupGatherer&);
extern template void insert_sort_values(const uint64_t*, int32_t*, int, GroupGatherer&);

extern template void insert_sort_keys_str(const uint8_t*, const int32_t*, int32_t, int32_t*, int32_t*, int, GroupGatherer&);
extern template void insert_sort_values_str(const uint8_t*, const int32_t*, int32_t, int32_t*, int, GroupGatherer&);

extern template int compare_offstrings(const uint8_t*, int32_t, int32_t, int32_t, int32_t);

extern template void GroupGatherer::from_data(const uint8_t*,  int32_t*, size_t);
extern template void GroupGatherer::from_data(const uint16_t*, int32_t*, size_t);
extern template void GroupGatherer::from_data(const uint32_t*, int32_t*, size_t);
extern template void GroupGatherer::from_data(const uint64_t*, int32_t*, size_t);
extern template void GroupGatherer::from_data(const uint8_t*, const int32_t*, int32_t, int32_t*, size_t);


#endif
