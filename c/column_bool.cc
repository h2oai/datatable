//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018-2019
//------------------------------------------------------------------------------
#include "python/_all.h"
#include "column/sentinel_fw.h"
#include "datatablemodule.h"
namespace dt {


BoolColumn::BoolColumn(ColumnImpl*&& other)
  : FwColumn<int8_t>(std::move(other)) {}


BoolColumn::BoolColumn(size_t nrows) : FwColumn<int8_t>(nrows) {
  stype_ = SType::BOOL;
}


BoolColumn::BoolColumn(size_t nrows, Buffer&& mem)
  : FwColumn<int8_t>(nrows, std::move(mem)) {
  stype_ = SType::BOOL;
}


ColumnImpl* BoolColumn::clone() const {
  return new BoolColumn(nrows_, Buffer(mbuf_));
}


bool BoolColumn::get_element(size_t i, int32_t* x) const {
  int8_t value = static_cast<const int8_t*>(mbuf_.rptr())[i];
  *x = static_cast<int32_t>(value);
  return !ISNA<int8_t>(value);
}


} // namespace dt
