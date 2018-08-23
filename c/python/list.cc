//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#include "python/list.h"
#include "utils/exceptions.h"

namespace py {



list::list() { v = nullptr; }

list::list(PyObject* src) : oobj(src) {
  is_list = PyList_Check(src);
}

list::list(size_t n) {
  is_list = true;
  v = PyList_New(static_cast<Py_ssize_t>(n));
  if (!v) throw PyError();
}



list::operator bool() const { return v != nullptr; }

size_t list::size() const {
  return static_cast<size_t>(Py_SIZE(v));
}

obj list::operator[](size_t i) const {
  return obj(is_list? PyList_GET_ITEM(v, i)
                    : PyTuple_GET_ITEM(v, i));
}

void list::set(int64_t i, const _obj& value) {
  if (is_list) {
    PyList_SET_ITEM(v, i, value.to_pyobject_newref());
  } else {
    PyTuple_SET_ITEM(v, i, value.to_pyobject_newref());
  }
}

void list::set(int64_t i, oobj&& value) {
  if (is_list) {
    PyList_SET_ITEM(v, i, value.release());
  } else {
    PyTuple_SET_ITEM(v, i, value.release());
  }
}

void list::set(size_t i, const _obj& value) {
  set(static_cast<int64_t>(i), value);
}

void list::set(size_t i, oobj&& value) {
  set(static_cast<int64_t>(i), std::move(value));
}

void list::set(int i, const _obj& value) {
  set(static_cast<int64_t>(i), value);
}

void list::set(int i, oobj&& value) {
  set(static_cast<int64_t>(i), std::move(value));
}


}
