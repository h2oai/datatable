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


PyyListEntry& PyyListEntry::operator=(py::oobj&& o) {
  PyObject* item = o.release();
  PyList_SET_ITEM(list, i, item);
  return *this;
}


PyObject* PyyListEntry::get() const {
  xassert(list);
  return PyList_GET_ITEM(list, i);
}


PyyListEntry::operator py::oobj() const { return py::oobj(get()); }
PyyListEntry::operator py::bobj() const { return py::bobj(get()); }
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


list::list() { obj = nullptr; }

list::list(const PyyList& src) : oobj(src.list) {}

PyyList list::to_pyylist() const { return PyyList(obj); }


list::operator bool() const { return obj != nullptr; }

size_t list::size() const {
  return static_cast<size_t>(Py_SIZE(obj));
}

bobj list::operator[](size_t i) const {
  return bobj(PyList_GET_ITEM(obj, i));
}


}
