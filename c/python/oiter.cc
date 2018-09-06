//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#include "python/oiter.h"

namespace py {


//------------------------------------------------------------------------------
// Constructors
//------------------------------------------------------------------------------

oiter::oiter() : oobj(nullptr) {}

oiter::oiter(PyObject* src) : oobj(PyObject_GetIter(src)) {}

oiter::oiter(const oiter& other) : oobj(other) {}

oiter::oiter(oiter&& other) : oobj(std::move(other)) {}

oiter& oiter::operator=(const oiter& other) {
  oobj::operator=(other);
  return *this;
}

oiter& oiter::operator=(oiter&& other) {
  oobj::operator=(std::move(other));
  return *this;
}



//------------------------------------------------------------------------------
// Iteration
//------------------------------------------------------------------------------

iter_iterator oiter::begin() const {
  return iter_iterator(v);
}

iter_iterator oiter::end() const {
  return iter_iterator(nullptr);
}

size_t oiter::size() const {
  try {
    py::oobj res = invoke("__length_hint__", "()");
    return static_cast<size_t>(res.to_int64_strict());
  } catch (const std::exception&) {
    return size_t(-1);
  }
}



iter_iterator::iter_iterator(PyObject* d)
  : iter(d), next_value(nullptr)
{
  advance();
}

iter_iterator& iter_iterator::operator++() {
  advance();
  return *this;
}

py::obj iter_iterator::operator*() const {
  return next_value;
}

bool iter_iterator::operator==(const iter_iterator& other) const {
  return (iter == other.iter);
}

bool iter_iterator::operator!=(const iter_iterator& other) const {
  return (iter != other.iter);
}

void iter_iterator::advance() {
  if (!iter) return;
  PyObject* res = PyIter_Next(iter.to_borrowed_ref());
  if (res) {
    next_value = py::oobj::from_new_reference(res);
  } else {
    iter = py::oobj(nullptr);
    next_value = py::oobj(nullptr);
  }
}

}  // namespace py
