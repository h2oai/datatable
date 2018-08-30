//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#include "python/dict.h"

namespace py {


//------------------------------------------------------------------------------
// Constructors
//------------------------------------------------------------------------------

odict::odict() {
  v = PyDict_New();
  if (!v) throw PyError();
}

odict::odict(PyObject* src) : oobj(src) {}

odict::odict(const odict& other) : oobj(other) {}

odict::odict(odict&& other) : oobj(std::move(other)) {}

odict& odict::operator=(const odict& other) {
  oobj::operator=(other);
  return *this;
}

odict& odict::operator=(odict&& other) {
  oobj::operator=(std::move(other));
  return *this;
}



//------------------------------------------------------------------------------
// Element accessors
//------------------------------------------------------------------------------

size_t odict::size() const {
  return static_cast<size_t>(PyDict_Size(v));
}

bool odict::has(_obj key) const {
  PyObject* _key = key.to_borrowed_ref();
  return PyDict_GetItem(v, _key) != nullptr;
}

obj odict::get(_obj key) const {
  // PyDict_GetItem returns a borrowed ref; or NULL if key is not present
  PyObject* _key = key.to_borrowed_ref();
  return obj(PyDict_GetItem(v, _key));
}

void odict::set(_obj key, _obj val) {
  // PyDict_SetItem INCREFs both key and value internally
  PyObject* _key = key.to_borrowed_ref();
  PyObject* _val = val.to_borrowed_ref();
  int r = PyDict_SetItem(v, _key, _val);
  if (r) throw PyError();
}


dict_iterator odict::begin() const {
  return dict_iterator(v, 0);
}

dict_iterator odict::end() const {
  return dict_iterator(v, -1);
}



//------------------------------------------------------------------------------
// dict_iterator
//------------------------------------------------------------------------------

dict_iterator::dict_iterator(PyObject* p, Py_ssize_t i0)
  : dict(p), pos(i0), curr_value(obj(nullptr), obj(nullptr))
{
  advance();
}

dict_iterator& dict_iterator::operator++() {
  advance();
  return *this;
}

dict_iterator::value_type dict_iterator::operator*() const {
  return curr_value;
}

bool dict_iterator::operator==(const dict_iterator& other) const {
  return (pos == other.pos);
}

bool dict_iterator::operator!=(const dict_iterator& other) const {
  return (pos != other.pos);
}

void dict_iterator::advance() {
  if (pos == -1) return;
  PyObject *key, *value;
  if (PyDict_Next(dict, &pos, &key, &value)) {
    curr_value = value_type(py::obj(key), py::obj(value));
  } else {
    pos = -1;
  }
}


}
