//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#include "sort.h"
#include <utility>  // std::move
#include "utils/assert.h"



GroupGatherer::GroupGatherer()
  : groups(nullptr) {}


void GroupGatherer::init(int32_t* data, int32_t cumsize0) {
  groups = data;
  count = 0;
  cumsize = cumsize0;
}


void GroupGatherer::push(size_t grp) {
  cumsize += static_cast<int32_t>(grp);
  groups[count++] = cumsize;
}


template <typename T, typename V>
void GroupGatherer::from_data(const T* data, V* o, size_t n) {
  if (n == 0) return;
  T curr_value = data[o[0]];
  size_t lasti = 0;
  for (size_t i = 1; i < n; ++i) {
    T xi = data[o[i]];
    if (xi != curr_value) {
      push(i - lasti);
      curr_value = xi;
      lasti = i;
    }
  }
  push(n - lasti);
}


template <typename T, typename V>
void GroupGatherer::from_data(
  const uint8_t* strdata, const T* stroffs, T start, V* o, size_t n)
{
  if (n == 0) return;
  T olast0 = std::abs(stroffs[o[0] - 1]) + start;
  T olast1 = stroffs[o[0]];
  size_t lasti = 0;
  for (size_t i = 1; i < n; ++i) {
    T ocurr0 = std::abs(stroffs[o[i] - 1]) + start;
    T ocurr1 = stroffs[o[i]];
    if (compare_offstrings(strdata, olast0, olast1, ocurr0, ocurr1)) {
      push(i - lasti);
      olast0 = ocurr0;
      olast1 = ocurr1;
      lasti = i;
    }
  }
  push(n - lasti);
}


void GroupGatherer::from_chunks(radix_range* rrmap, size_t nradixes) {
  xassert(count == 0);
  size_t dest_off = 0;
  for (size_t i = 0; i < nradixes; ++i) {
    size_t grp_size = rrmap[i].size;
    if (!grp_size) continue;
    size_t grp_off = rrmap[i].offset;
    if (grp_off != dest_off) {
      std::memmove(groups + dest_off, groups + grp_off,
                   grp_size * sizeof(int32_t));
    }
    dest_off += grp_size;
  }
  count = static_cast<int32_t>(dest_off);
  cumsize = groups[count - 1];
}


void GroupGatherer::from_histogram(
  size_t* histogram, size_t nchunks, size_t nradixes)
{
  xassert(count == 0);
  size_t* rrendoffsets = histogram + (nchunks - 1) * nradixes;
  int32_t off0 = 0;
  for (size_t i = 0; i < nradixes; ++i) {
    int32_t off1 = static_cast<int32_t>(rrendoffsets[i]);
    if (off1 > off0) {
      groups[count++] = cumsize + off1;
      off0 = off1;
    }
  }
  cumsize = groups[count - 1];
}


template void GroupGatherer::from_data(const uint8_t*,  int32_t*, size_t);
template void GroupGatherer::from_data(const uint16_t*, int32_t*, size_t);
template void GroupGatherer::from_data(const uint32_t*, int32_t*, size_t);
template void GroupGatherer::from_data(const uint64_t*, int32_t*, size_t);
template void GroupGatherer::from_data(const uint8_t*, const int32_t*, int32_t, int32_t*, size_t);
