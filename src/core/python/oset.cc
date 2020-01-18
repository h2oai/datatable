//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#include "python/oset.h"

namespace py {


//------------------------------------------------------------------------------
// Constructors
//------------------------------------------------------------------------------

oset::oset() {
  v = PySet_New(nullptr);
  if (!v) throw PyError();
}

oset::oset(const oset& other) : oobj(other) {}

oset::oset(oset&& other) : oobj(std::move(other)) {}

oset& oset::operator=(const oset& other) {
  oobj::operator=(other);
  return *this;
}

oset& oset::operator=(oset&& other) {
  oobj::operator=(std::move(other));
  return *this;
}

oset::oset(PyObject* src) : oobj(src) {}



//------------------------------------------------------------------------------
// Accessors
//------------------------------------------------------------------------------

size_t oset::size() const {
  return static_cast<size_t>(PySet_Size(v));
}

/**
 * Test whether the `key` is present in the set. If the lookup raises an error
 * (for example if key is not hashable), discard it and return `false`.
 */
bool oset::has(const _obj& key) const {
  int ret = PySet_Contains(v, key.to_borrowed_ref());
  if (ret == -1) PyErr_Clear();
  return (ret == 1);
}

/**
 * Insert the provided `key` into the set.
 */
void oset::add(const _obj& key) {
  int ret = PySet_Add(v, key.to_borrowed_ref());
  if (ret == -1) throw PyError();
}



}  // namespace py
