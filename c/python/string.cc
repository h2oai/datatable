//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#include "python/string.h"
#include "utils/exceptions.h"

namespace py {


//------------------------------------------------------------------------------
// Constructors
//------------------------------------------------------------------------------

ostring::ostring() : oobj() {}

ostring::ostring(PyObject* src) : oobj(src) {}

ostring ostring::from_new_reference(PyObject* ref) {
  ostring res;
  res.v = ref;
  return res;
}


ostring::ostring(const std::string& s) {
  Py_ssize_t slen = static_cast<Py_ssize_t>(s.size());
  v = PyUnicode_FromStringAndSize(s.data(), slen);
}

ostring::ostring(const char* str, size_t len) {
  Py_ssize_t slen = static_cast<Py_ssize_t>(len);
  v = PyUnicode_FromStringAndSize(str, slen);
}

ostring::ostring(const char* str) {
  Py_ssize_t slen = static_cast<Py_ssize_t>(std::strlen(str));
  v = PyUnicode_FromStringAndSize(str, slen);
}


ostring::ostring(const ostring& other) : oobj(other) {}

ostring::ostring(ostring&& other) : oobj(std::move(other)) {}

ostring& ostring::operator=(const ostring& other) {
  oobj::operator=(other);
  return *this;
}

ostring& ostring::operator=(ostring&& other) {
  oobj::operator=(std::move(other));
  return *this;
}



}  // namespace py
