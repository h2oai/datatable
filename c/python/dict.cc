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
// odict
//------------------------------------------------------------------------------

odict::odict() {
  v = PyDict_New();
}


obj odict::get(obj key) const {
  // PyDict_GetItem returns a borrowed ref; or NULL if key is not present
  return obj(PyDict_GetItem(v, key.to_borrowed_ref()));
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
