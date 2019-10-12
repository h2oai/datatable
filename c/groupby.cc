//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#include "groupby.h"
#include "utils/exceptions.h"


Groupby::Groupby() : n(0) {}


Groupby::Groupby(size_t _n, Buffer&& _offs) {
  if (_offs.size() < sizeof(int32_t) * (_n + 1)) {
    throw RuntimeError() << "Cannot create groupby for " << _n << " groups "
        "from memory buffer of size " << _offs.size();
  }
  if (_offs.get_element<int32_t>(0) != 0) {
    throw RuntimeError() << "Invalid memory buffer for the Groupby: its first "
        "element is not 0.";
  }
  offsets = std::move(_offs);
  n = _n;
}


Groupby Groupby::single_group(size_t nrows) {
  Buffer mr = Buffer::mem(2 * sizeof(int32_t));
  mr.set_element<int32_t>(0, 0);
  mr.set_element<int32_t>(1, static_cast<int32_t>(nrows));
  return Groupby(1, std::move(mr));
}


const int32_t* Groupby::offsets_r() const {
  return static_cast<const int32_t*>(offsets.rptr());
}


size_t Groupby::ngroups() const {
  return n;
}

size_t Groupby::size() const noexcept {
  return n;
}

size_t Groupby::last_offset() const noexcept {
  auto offs = offsets_r();
  return offs? static_cast<size_t>(offs[n]) : 0;
}

Groupby::operator bool() const {
  return n != 0;
}

void Groupby::get_group(size_t i, size_t* i0, size_t* i1) const {
  const int32_t* offsets_array = offsets_r();
  *i0 = static_cast<size_t>(offsets_array[i]);
  *i1 = static_cast<size_t>(offsets_array[i + 1]);
}


RowIndex Groupby::ungroup_rowindex() {
  const int32_t* offs = offsets_r();
  if (!offs) return RowIndex();
  int32_t nrows = offs[n];
  arr32_t indices(static_cast<size_t>(nrows));
  int32_t* data = indices.data();
  int32_t j = 0;
  for (size_t i = 0; i < n; ++i) {
    int32_t upto = offs[i + 1];
    int32_t ii = static_cast<int32_t>(i);
    while (j < upto) data[j++] = ii;
  }
  return RowIndex(std::move(indices), /* sorted = */ true);
}
