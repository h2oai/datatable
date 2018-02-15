//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#include "python/list.h"
#include "utils/exceptions.h"



//------------------------------------------------------------------------------
// Constructors
//------------------------------------------------------------------------------

PyyList::PyyList() : list(nullptr) {}

PyyList::PyyList(size_t n) {
  list = PyList_New(static_cast<Py_ssize_t>(n));
  if (!list) throw PyError();
}

PyyList::PyyList(PyObject* src) {
  if (!src) throw PyError();
  if (src == Py_None) {
    list = nullptr;
  } else {
    list = src;
    if (!PyList_Check(src)) {
      throw TypeError() << "Object " << src << " is not a list";
    }
    Py_INCREF(list);
  }
}

PyyList::PyyList(const PyyList& other) : PyyList(other.list) {}

PyyList::PyyList(PyyList&& other) : PyyList() {
  swap(*this, other);
}

PyyList::~PyyList() {
  Py_XDECREF(list);
}


void swap(PyyList& first, PyyList& second) noexcept {
  std::swap(first.list, second.list);
}



//------------------------------------------------------------------------------
// PyyList API
//------------------------------------------------------------------------------

size_t PyyList::size() const noexcept {
  return static_cast<size_t>(PyList_GET_SIZE(list));
}


PyyList::operator bool() const noexcept {
  return (list != nullptr);
}


PyyListEntry PyyList::operator[](size_t i) const {
  return PyyListEntry(list, static_cast<Py_ssize_t>(i));
}


PyObject* PyyList::release() {
  PyObject* o = list;
  list = nullptr;
  return o;
}



//------------------------------------------------------------------------------
// PyyListEntry API
//------------------------------------------------------------------------------

PyyListEntry::PyyListEntry(PyObject* pylist, Py_ssize_t index)
  : list(pylist), i(index) {}


PyyListEntry& PyyListEntry::operator=(PyObject* s) {
  PyList_SET_ITEM(list, i, s);
  return *this;
}


PyyListEntry& PyyListEntry::operator=(const PyObj& o) {
  PyObject* item = o.as_pyobject();
  PyList_SetItem(list, i, item);
  return *this;
}


PyyListEntry::operator PyObj() const {
  return PyObj(PyList_GET_ITEM(list, i));
}
