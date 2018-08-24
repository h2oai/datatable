//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#include "python/arg.h"
#include "python/args.h"        // py::PKArgs
#include "python/int.h"
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

int32_t   Arg::to_int32_strict() const { return pyobj.to_int32_strict(*this); }
int64_t   Arg::to_int64_strict() const { return pyobj.to_int64_strict(*this); }
py::olist Arg::to_pylist()       const { return pyobj.to_pylist(*this); }



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
