//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#include "python/float.h"
#include "utils/exceptions.h"

namespace py {


//------------------------------------------------------------------------------
// Constructors
//------------------------------------------------------------------------------

ofloat::ofloat(double src) {
  v = PyFloat_FromDouble(src);  // new ref
}

ofloat::ofloat(float src) : ofloat(static_cast<double>(src)) {}


// private constructors
ofloat::ofloat(const robj& src) : oobj(src) {}
ofloat::ofloat(const oobj& src) : oobj(src) {}



//------------------------------------------------------------------------------
// Value conversions
//------------------------------------------------------------------------------

template<>
float ofloat::value() const {
  if (!v) return GETNA<float>();
  return static_cast<float>(PyFloat_AS_DOUBLE(v));
}

template<>
double ofloat::value() const {
  if (!v) return GETNA<double>();
  return PyFloat_AS_DOUBLE(v);
}



}  // namespace py
