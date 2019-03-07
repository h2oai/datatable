//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#include <Python.h>
#include "python/_all.h"
#include "utils/parallel.h"
#include "column.h"
#include "datatablemodule.h"




SType BoolColumn::stype() const noexcept {
  return SType::BOOL;
}

py::oobj BoolColumn::get_value_at_index(size_t i) const {
  size_t j = ri[i];
  int8_t x = elements_r()[j];
  return ISNA(x)? py::None() : x? py::True() : py::False();
}



//------------------------------------------------------------------------------
// Stats
//------------------------------------------------------------------------------

BooleanStats* BoolColumn::get_stats() const {
  if (stats == nullptr) stats = new BooleanStats();
  return static_cast<BooleanStats*>(stats);
}

int8_t  BoolColumn::min() const  { return get_stats()->min(this); }
int8_t  BoolColumn::max() const  { return get_stats()->max(this); }
int8_t  BoolColumn::mode() const { return get_stats()->mode(this); }
int64_t BoolColumn::sum() const  { return get_stats()->sum(this); }
double  BoolColumn::mean() const { return get_stats()->mean(this); }
double  BoolColumn::sd() const   { return get_stats()->stdev(this); }
