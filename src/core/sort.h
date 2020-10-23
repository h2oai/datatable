//------------------------------------------------------------------------------
// Copyright 2018-2020 H2O.ai
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
#ifndef dt_SORT_h
#define dt_SORT_h
#include <vector>
#include <cstddef>
#include "_dt.h"


struct radix_range {
  size_t size;
  size_t offset;
};


// Sort flags (can be OR-ed together)
enum SortFlag : int {
  NONE =  0,
  NA_LAST = 1,
  DESCENDING = 2,
  SORT_ONLY = 4,
};

enum NaPosition : int {
  INVALID = 0,
  FIRST =  1,
  LAST = 2,
  REMOVE = 3,
};

static inline SortFlag operator|(SortFlag a, SortFlag b) {
  return static_cast<SortFlag>(static_cast<int>(a) | static_cast<int>(b));
}


// Main sorting function
RiGb group(const std::vector<Column>& columns,
           const std::vector<SortFlag>& flags,
           NaPosition na_pos = NaPosition::FIRST);



// Called during module initialization
void sort_init_options();


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
 * from_data(column, indices, n)
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
    void init(int32_t* data, int32_t cumsize0, size_t count_ = 0);

    int32_t* data() const { return groups; }
    size_t   size() const { return static_cast<size_t>(count); }
    int32_t  cumulative_size() const { return cumsize; }
    operator bool() const { return !!groups; }

    void push(size_t grp);

    template <typename T, typename V>
    void from_data(const T*, const V*, size_t);

    template <typename V>
    void from_data(const Column&, const V*, size_t);

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

template <typename V>
void insert_sort_keys_str(const Column&, size_t, V*, V*, int, GroupGatherer&, bool, NaPosition na_pos);

template <typename V>
void insert_sort_values_str(const Column&, size_t, V*, int, GroupGatherer&, bool, NaPosition na_pos);

template <int R, int NA>
int compare_strings(const dt::CString& a, bool a_isna,
                    const dt::CString& b, bool b_isna, size_t strstart);


extern template void insert_sort_keys(const uint8_t*,  int32_t*, int32_t*, int, GroupGatherer&);
extern template void insert_sort_keys(const uint16_t*, int32_t*, int32_t*, int, GroupGatherer&);
extern template void insert_sort_keys(const uint32_t*, int32_t*, int32_t*, int, GroupGatherer&);
extern template void insert_sort_keys(const uint64_t*, int32_t*, int32_t*, int, GroupGatherer&);

extern template void insert_sort_values(const uint8_t*,  int32_t*, int, GroupGatherer&);
extern template void insert_sort_values(const uint16_t*, int32_t*, int, GroupGatherer&);
extern template void insert_sort_values(const uint32_t*, int32_t*, int, GroupGatherer&);
extern template void insert_sort_values(const uint64_t*, int32_t*, int, GroupGatherer&);

extern template void insert_sort_keys_str(const Column&, size_t, int32_t*, int32_t*, int, GroupGatherer&, bool, NaPosition na_pos);
extern template void insert_sort_values_str(const Column&, size_t, int32_t*, int, GroupGatherer&, bool, NaPosition na_pos);

extern template int compare_strings<1,1>(const dt::CString&, bool, const dt::CString&, bool, size_t);
extern template int compare_strings<1,-1>(const dt::CString&, bool, const dt::CString&, bool, size_t);
extern template int compare_strings<-1,1>(const dt::CString&, bool, const dt::CString&, bool, size_t);
extern template int compare_strings<-1,-1>(const dt::CString&, bool, const dt::CString&, bool, size_t);

extern template void GroupGatherer::from_data(const uint8_t*,  const int32_t*, size_t);
extern template void GroupGatherer::from_data(const uint16_t*, const int32_t*, size_t);
extern template void GroupGatherer::from_data(const uint32_t*, const int32_t*, size_t);
extern template void GroupGatherer::from_data(const uint64_t*, const int32_t*, size_t);
extern template void GroupGatherer::from_data(const Column&, const int32_t*, size_t);
extern template void GroupGatherer::from_data(const Column&, const int64_t*, size_t);

#endif
