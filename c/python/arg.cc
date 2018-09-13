//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#include "python/arg.h"
#include "python/args.h"        // py::PKArgs
#include "utils/exceptions.h"

namespace py {

std::string _nth(size_t i);



//------------------------------------------------------------------------------
// Construction / initialization
//------------------------------------------------------------------------------

Arg::Arg() : pos(0), parent(nullptr), pyobj(nullptr) {}

Arg::~Arg() {}

void Arg::init(size_t i, PKArgs* args) {
  pos = i;
  parent = args;
}


void Arg::set(PyObject* value) {
  pyobj = py::obj(value);
}


const std::string& Arg::name() const {
  if (cached_name.empty()) {
    cached_name = parent->make_arg_name(pos);
  }
  return cached_name;
}



//------------------------------------------------------------------------------
// Type checks
//------------------------------------------------------------------------------

bool Arg::is_undefined()     const { return pyobj.is_undefined(); }
bool Arg::is_none()          const { return pyobj.is_none(); }
bool Arg::is_ellipsis()      const { return pyobj.is_ellipsis(); }
bool Arg::is_int()           const { return pyobj.is_int(); }
bool Arg::is_float()         const { return pyobj.is_float(); }
bool Arg::is_list()          const { return pyobj.is_list(); }
bool Arg::is_tuple()         const { return pyobj.is_tuple(); }
bool Arg::is_list_or_tuple() const { return pyobj.is_list_or_tuple(); }
bool Arg::is_dict()          const { return pyobj.is_dict(); }
bool Arg::is_string()        const { return pyobj.is_string(); }
bool Arg::is_range()         const { return pyobj.is_range(); }



//------------------------------------------------------------------------------
// Type conversions
//------------------------------------------------------------------------------

py::list  Arg::to_pylist() const { return pyobj.to_pylist(*this); }

/*
Arg::operator int32_t() const {
  _check_missing();
  if (!PyLong_Check(pyobj)) {
    throw TypeError() << name() << " should be an integer";
  }
  int overflow;
  long value = PyLong_AsLongAndOverflow(pyobj, &overflow);
  int32_t res = static_cast<int32_t>(value);
  if (overflow || value != static_cast<long>(res)) {
    throw TypeError() << name() << " is too large for an int32: " << pyobj;
  }
  return res;
}


Arg::operator int64_t() const {
  static_assert(sizeof(int64_t) == sizeof(long), "Unexpected size of long");
  _check_missing();
  if (!PyLong_Check(pyobj)) {
    throw TypeError() << name() << " should be an integer";
  }
  int overflow;
  long value = PyLong_AsLongAndOverflow(pyobj, &overflow);
  if (overflow) {
    throw TypeError() << name() << " is too large for an int64: " << pyobj;
  }
  return value;
}


Arg::operator list() const {
  _check_missing();
  _check_list_or_tuple();
  return list(pyobj);
}


std::vector<std::string> Arg::to_list_of_strs() const {
  _check_missing();
  _check_list_or_tuple();
  auto size = static_cast<size_t>(Py_SIZE(pyobj));
  std::vector<std::string> res(size);
  for (size_t i = 0; i < size; ++i) {
    PyObject* item = PyList_GET_ITEM(pyobj, i);  // works for tuples too
    if (!PyUnicode_Check(item)) {
      auto item_type = reinterpret_cast<PyObject*>(Py_TYPE(item));
      throw TypeError() << name() << " should be a list of strings; but its "
          << _nth(i + 1) << " element was " << item_type;
    }
    Py_ssize_t str_size;
    const char* str = PyUnicode_AsUTF8AndSize(item, &str_size);
    if (!str) throw PyError();
    res[i] = std::string(str, static_cast<size_t>(str_size));
  }
  return res;
}
*/


//------------------------------------------------------------------------------
// Error messages
//------------------------------------------------------------------------------

Error Arg::error_not_list(PyObject* src) const {
  return TypeError() << name() << " should be a list or tuple, instead got "
      << Py_TYPE(src);
}



//------------------------------------------------------------------------------
// Misc
//------------------------------------------------------------------------------

void Arg::print() const {
  PyObject_Print(pyobj.v, stdout, Py_PRINT_RAW);
  std::printf("\n");
}

void Arg::_check_missing() const {
  if (pyobj.is_undefined()) {
    throw TypeError() << " is missing";
  }
}

void Arg::_check_list_or_tuple() const {
  if (!(pyobj.is_list() || pyobj.is_tuple())) {
    throw TypeError() << name() << " should be a list";
  }
}

std::string _nth(size_t i) {
  return std::to_string(i) + ((i / 10) % 10 == 1 ? "th" :
                              i % 10 == 1 ? "st" :
                              i % 10 == 2 ? "nd" :
                              i % 10 == 3 ? "rd" : "th");
}


}  // namespace py
