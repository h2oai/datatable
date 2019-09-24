//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#include "python/_all.h"
#include "column_impl.h"
#include "datatablemodule.h"


BoolColumn::BoolColumn(size_t nrows) : FwColumn<int8_t>(nrows) {
  _stype = SType::BOOL;
}

BoolColumn::BoolColumn(size_t nrows, MemoryRange&& mem)
  : FwColumn<int8_t>(nrows, std::move(mem)) {
  _stype = SType::BOOL;
}


bool BoolColumn::get_element(size_t i, int32_t* x) const {
  int8_t value = elements_r()[i];
  *x = static_cast<int32_t>(value);
  return !ISNA<int8_t>(value);
}
