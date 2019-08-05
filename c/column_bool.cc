//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#include "python/_all.h"
#include "column.h"
#include "datatablemodule.h"


BoolColumn::BoolColumn(size_t nrows) : FwColumn<int8_t>(nrows) {
  _stype = SType::BOOL;
}

BoolColumn::BoolColumn(size_t nrows, MemoryRange&& mem)
  : FwColumn<int8_t>(nrows, std::move(mem)) {
  _stype = SType::BOOL;
}


bool BoolColumn::get_element(size_t i, int32_t* x) const {
  size_t j = ri[i];
  if (j == RowIndex::NA) return true;
  int8_t value = elements_r()[j];
  *x = static_cast<int32_t>(value);
  return ISNA<int8_t>(value);
}



//------------------------------------------------------------------------------
// Stats
//------------------------------------------------------------------------------

// static inline BooleanStats* get_bool_stats(const Column* col) {
//   return static_cast<BooleanStats*>(col->get_stats());
// }

// int8_t  BoolColumn::min() const  { return downcast<int8_t>(get_bool_stats(this)->min(this)); }
// int8_t  BoolColumn::max() const  { return downcast<int8_t>(get_bool_stats(this)->max(this)); }
// int8_t  BoolColumn::mode() const { return downcast<int8_t>(get_bool_stats(this)->mode(this)); }
// int64_t BoolColumn::sum() const  { return get_bool_stats(this)->sum(this); }
// double  BoolColumn::mean() const { return get_bool_stats(this)->mean(this); }
// double  BoolColumn::sd() const   { return get_bool_stats(this)->stdev(this); }
