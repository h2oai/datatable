//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#include "python/orange.h"
#include "python/int.h"
#include "python/tuple.h"

namespace py {


//------------------------------------------------------------------------------
// Constructors
//------------------------------------------------------------------------------

orange::orange(int64_t start, int64_t stop, int64_t step) {
  py::otuple args(3);
  args.set(0, ISNA(start)? py::None() : py::oint(start));
  args.set(1, ISNA(stop) ? py::None() : py::oint(stop));
  args.set(2, ISNA(step) ? py::None() : py::oint(step));
  v = PyObject_CallObject(reinterpret_cast<PyObject*>(&PyRange_Type),
                          args.to_borrowed_ref());
  if (!v) throw PyError();
}

orange::orange(const orange& other) : oobj(other) {}

orange::orange(orange&& other) : oobj(std::move(other)) {}

orange& orange::operator=(const orange& other) {
  oobj::operator=(other);
  return *this;
}

orange& orange::operator=(orange&& other) {
  oobj::operator=(std::move(other));
  return *this;
}

orange::orange(PyObject* src) : oobj(src) {}



//------------------------------------------------------------------------------
// Accessors
//------------------------------------------------------------------------------

int64_t orange::start() const {
  return get_attr("start").to_int64();
}

int64_t orange::stop() const {
  return get_attr("stop").to_int64();
}

int64_t orange::step() const {
  return get_attr("step").to_int64();
}



}  // namespace py
