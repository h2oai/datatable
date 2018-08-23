//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#include "python/tuple.h"

namespace py {


otuple::otuple(int n) : otuple(static_cast<int64_t>(n)) {}

otuple::otuple(size_t n) : otuple(static_cast<int64_t>(n)) {}

otuple::otuple(int64_t n) {
  v = PyTuple_New(n);
}


size_t otuple::size() const {
  return static_cast<size_t>(Py_SIZE(v));
}

obj otuple::get(size_t i) const {
  // PyTuple_GET_ITEM returns a borrowed reference
  return obj(PyTuple_GET_ITEM(v, i));
}

void otuple::set(size_t i, const _obj& value) {
  // PyTuple_SET_ITEM "steals" a reference to the last argument
  PyTuple_SET_ITEM(v, i, value.to_pyobject_newref());
}

void otuple::set(size_t i, oobj&& value) {
  PyTuple_SET_ITEM(v, i, value.release());
}


}  // namespace py
