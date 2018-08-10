//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#include "python/string.h"
#include "utils/exceptions.h"



//------------------------------------------------------------------------------
// Constructors
//------------------------------------------------------------------------------

PyyString::PyyString() : obj(nullptr) {}

PyyString::PyyString(const std::string& s) {
  Py_ssize_t slen = static_cast<Py_ssize_t>(s.size());
  obj = PyUnicode_FromStringAndSize(s.data(), slen);
}

PyyString::PyyString(const char* str, size_t len) {
  Py_ssize_t slen = static_cast<Py_ssize_t>(len);
  obj = PyUnicode_FromStringAndSize(str, slen);
}

PyyString::PyyString(const char* str) {
  Py_ssize_t slen = static_cast<Py_ssize_t>(std::strlen(str));
  obj = PyUnicode_FromStringAndSize(str, slen);
}


PyyString::PyyString(PyObject* src) {
  if (!src) throw PyError();
  if (src == Py_None) {
    obj = nullptr;
  } else {
    obj = src;
    if (!PyUnicode_Check(src)) {
      throw TypeError() << "Object " << src << " is not a string";
    }
    Py_INCREF(src);
  }
}

PyyString::PyyString(const PyyString& other) {
  obj = other.obj;
  Py_INCREF(obj);
}

PyyString::PyyString(PyyString&& other) : PyyString() {
  swap(*this, other);
}

PyyString::~PyyString() {
  Py_XDECREF(obj);
}


void swap(PyyString& first, PyyString& second) noexcept {
  std::swap(first.obj, second.obj);
}



//------------------------------------------------------------------------------
// Public API
//------------------------------------------------------------------------------

PyyString::operator py::oobj() && {
  PyObject* t = obj;
  obj = nullptr;
  return py::oobj::from_new_reference(t);
}

PyObject* PyyString::release() {
  PyObject* t = obj;
  obj = nullptr;
  return t;
}
