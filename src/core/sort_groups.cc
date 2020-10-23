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
#include <cstring>         // std::memmove
#include <utility>         // std::move
#include "utils/assert.h"
#include "column.h"
#include "sort.h"



GroupGatherer::GroupGatherer()
  : groups(nullptr) {}


void GroupGatherer::init(int32_t* data, int32_t cumsize0, size_t count_) {
  groups = data;
  count = static_cast<int32_t>(count_);
  cumsize = cumsize0;
}


void GroupGatherer::push(size_t grp) {
  cumsize += static_cast<int32_t>(grp);
  groups[count++] = cumsize;
}


template <typename T, typename V>
void GroupGatherer::from_data(const T* data, const V* o, size_t n) {
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


template <typename V>
void GroupGatherer::from_data(const Column& column, const V* o, size_t n) {
  if (n == 0) return;
  dt::CString last_value, curr_value;
  bool last_valid, curr_valid;
  last_valid = column.get_element(static_cast<size_t>(o[0]), &last_value);
  size_t last_i = 0;
  for (size_t i = 1; i < n; ++i) {
    curr_valid = column.get_element(static_cast<size_t>(o[i]), &curr_value);
    if (compare_strings<1,1>(last_value, last_valid, curr_value, curr_valid, 0)) {
      push(i - last_i);
      last_i = i;
      last_valid = curr_valid;
      last_value = std::move(curr_value);
    }
  }
  push(n - last_i);
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
  xassert(count > 0);
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


template void GroupGatherer::from_data(const uint8_t*,  const int32_t*, size_t);
template void GroupGatherer::from_data(const uint16_t*, const int32_t*, size_t);
template void GroupGatherer::from_data(const uint32_t*, const int32_t*, size_t);
template void GroupGatherer::from_data(const uint64_t*, const int32_t*, size_t);
template void GroupGatherer::from_data(const Column&, const int32_t*, size_t);
template void GroupGatherer::from_data(const Column&, const int64_t*, size_t);
