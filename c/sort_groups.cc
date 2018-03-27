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



GroupGatherer::GroupGatherer(bool enabled)
  : _enabled(enabled)
{
  if (!enabled) return;
  groups.resize(16);
  groups[0] = 0;
  count = 1;
  total_size = 0;
}


void GroupGatherer::clear() {
  count = 1;
  total_size = 0;
}


void GroupGatherer::push(size_t grp) {
  if (count == groups.size()) groups.resize(2 * count);
  total_size += static_cast<int32_t>(grp);
  groups[count++] = total_size;
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


void GroupGatherer::from_groups(const GroupGatherer& gg) {
  if (count + gg.count > groups.size()) groups.resize((count + gg.count) * 2);
  for (size_t i = 1; i < gg.count; ++i) {
    groups[count++] = total_size + gg.groups[i];
  }
  total_size += gg.total_size;
  assert(total_size == groups[count - 1]);
}


arr32_t&& GroupGatherer::release() {
  groups.resize(count);
  return std::move(groups);
}



template void GroupGatherer::from_data(const uint8_t*,  int32_t*, size_t);
template void GroupGatherer::from_data(const uint16_t*, int32_t*, size_t);
template void GroupGatherer::from_data(const uint32_t*, int32_t*, size_t);
template void GroupGatherer::from_data(const uint64_t*, int32_t*, size_t);
template void GroupGatherer::from_data(const uint8_t*, const int32_t*, int32_t, int32_t*, size_t);
