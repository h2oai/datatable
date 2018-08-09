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


Arg::Arg()
  : pos(0), parent(nullptr), pyobj(nullptr), cached_name(nullptr) {}


void Arg::init(size_t i, PKArgs* args) {
  pos = i;
  parent = args;
}


void Arg::set(PyObject* value) {
  pyobj = value;
}


bool Arg::is_present() const {
  return (pyobj != nullptr);
}


const std::string& Arg::name() const {
  if (cached_name.empty()) {
    cached_name = parent->make_arg_name(pos);
  }
  return cached_name;
}



//------------------------------------------------------------------------------
// Type conversions
//------------------------------------------------------------------------------

Arg::operator int32_t() const {
  if (!PyLong_Check(pyobj)) {
    throw TypeError() << name() << " should be an integer";
  }
  int overflow;
  long value = PyLong_AsLongAndOverflow(pyobj, &overflow);
  int32_t res = static_cast<int32_t>(value);
  if (overflow || value != static_cast<long>(res)) {
    throw TypeError() << name() << " is too large to fit in "
        "an int32: " << pyobj;
  }
  return res;
}


Arg::operator int64_t() const {
  static_assert(sizeof(int64_t) == sizeof(long), "Unexpected size of long");
  if (!PyLong_Check(pyobj)) {
    throw TypeError() << name() << " should be an integer";
  }
  int overflow;
  long value = PyLong_AsLongAndOverflow(pyobj, &overflow);
  if (overflow) {
    throw TypeError() << name() << " is too large to fit in "
        "an int64: " << pyobj;
  }
  return value;
}



}  // namespace py
