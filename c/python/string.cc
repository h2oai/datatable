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

string::string() : obj(nullptr) {}

string::string(PyObject* src) {
  if (!src) throw PyError();
  if (src == Py_None) {
    obj = nullptr;
  } else {
    obj = src;
    if (!PyUnicode_Check(src)) {
      throw TypeError() << "Object " << src << " is not a string";
    }
  }
}

string::string(const string& other) {
  obj = other.obj;
}

void swap(string& first, string& second) noexcept {
  std::swap(first.obj, second.obj);
}



ostring::ostring() {}

ostring::ostring(const std::string& s) {
  Py_ssize_t slen = static_cast<Py_ssize_t>(s.size());
  obj = PyUnicode_FromStringAndSize(s.data(), slen);
}

ostring::ostring(const char* str, size_t len) {
  Py_ssize_t slen = static_cast<Py_ssize_t>(len);
  obj = PyUnicode_FromStringAndSize(str, slen);
}

ostring::ostring(const char* str) {
  Py_ssize_t slen = static_cast<Py_ssize_t>(std::strlen(str));
  obj = PyUnicode_FromStringAndSize(str, slen);
}

ostring::ostring(PyObject* src) : string(src) {
  Py_XINCREF(src);
}

ostring::ostring(const ostring& other) {
  obj = other.obj;
  Py_XINCREF(obj);
}

ostring::ostring(ostring&& other) : ostring() {
  swap(*this, other);
}

ostring::~ostring() {
  Py_XDECREF(obj);
}



//------------------------------------------------------------------------------
// Other
//------------------------------------------------------------------------------

PyObject* ostring::release() {
  PyObject* t = obj;
  obj = nullptr;
  return t;
}


}  // namespace py
