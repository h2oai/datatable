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

ofloat::ofloat() : oobj() {}

ofloat::ofloat(PyObject* src) : oobj(src) {}

ofloat::ofloat(const ofloat& other) : oobj(other) {}

ofloat::ofloat(ofloat&& other) : oobj(std::move(other)) {}

ofloat& ofloat::operator=(const ofloat& other) {
  oobj::operator=(other);
  return *this;
}

ofloat& ofloat::operator=(ofloat&& other) {
  oobj::operator=(std::move(other));
  return *this;
}

ofloat ofloat::from_new_reference(PyObject* ref) {
  ofloat res;
  res.v = ref;
  return res;
}


ofloat::ofloat(double src) {
  v = PyFloat_FromDouble(src);  // new ref
}

ofloat::ofloat(float src) {
  v = PyFloat_FromDouble(static_cast<double>(src));  // new ref
}



//------------------------------------------------------------------------------
// Value conversions
//------------------------------------------------------------------------------

template<typename T>
T ofloat::value() const {
  if (!v) return GETNA<T>();
  return static_cast<T>(PyFloat_AS_DOUBLE(v));
}



template float  ofloat::value() const;
template double ofloat::value() const;

}  // namespace py
