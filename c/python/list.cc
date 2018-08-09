//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#include "python/list.h"
#include "python/long.h"
#include "python/float.h"
#include "utils/assert.h"
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
  xassert(list);
  PyList_SET_ITEM(list, i, s);
  return *this;
}


PyyListEntry& PyyListEntry::operator=(const PyObj& o) {
  PyObject* item = o.as_pyobject();
  PyList_SetItem(list, i, item);
  return *this;
}


PyObject* PyyListEntry::get() const {
  xassert(list);
  return PyList_GET_ITEM(list, i);
}


PyyListEntry::operator PyObj() const {
  // Variable `entry` cannot be inlined, otherwise PyObj constructor will
  // assume that the reference to PyObject* can be stolen.
  PyObject* entry = get();  // borrowed ref
  return PyObj(entry);
}
PyyListEntry::operator PyyList() const { return PyyList(get()); }
PyyListEntry::operator PyyLong() const { return PyyLong(get()); }
PyyListEntry::operator PyyFloat() const { return PyyFloat(get()); }


PyObject* PyyListEntry::as_new_ref() const {
  PyObject* res = PyList_GET_ITEM(list, i);
  Py_XINCREF(res);
  return res;
}



//------------------------------------------------------------------------------
// py::List
//------------------------------------------------------------------------------
namespace py {


// This constructor is private -- only used from friend class `Arg`
List::List(PyObject* o) {
  xassert(o && (PyList_Check(o) || PyTuple_Check(o)));
  pyobj = o;
  Py_INCREF(pyobj);
}

List::List(const List& other) {
  pyobj = other.pyobj;
  Py_INCREF(pyobj);
}

List::List(List&& other) {
  pyobj = other.pyobj;
  other.pyobj = nullptr;
}

List::~List() {
  // pyobj can be nullptr if it was target of a move
  Py_XDECREF(pyobj);
}


size_t List::size() const {
  return static_cast<size_t>(Py_SIZE(pyobj));
}


}
