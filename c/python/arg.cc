//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#include "python/arg.h"
#include "python/args.h"        // py::PKArgs
#include "python/dict.h"
#include "python/int.h"
#include "python/list.h"
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
  pyobj = py::robj(value);
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
bool Arg::is_bool()          const { return pyobj.is_bool(); }
bool Arg::is_int()           const { return pyobj.is_int(); }
bool Arg::is_float()         const { return pyobj.is_float(); }
bool Arg::is_list()          const { return pyobj.is_list(); }
bool Arg::is_tuple()         const { return pyobj.is_tuple(); }
bool Arg::is_list_or_tuple() const { return pyobj.is_list_or_tuple(); }
bool Arg::is_dict()          const { return pyobj.is_dict(); }
bool Arg::is_string()        const { return pyobj.is_string(); }
bool Arg::is_range()         const { return pyobj.is_range(); }
bool Arg::is_frame()         const { return pyobj.is_frame(); }
bool Arg::is_pandas_frame()  const { return pyobj.is_pandas_frame(); }
bool Arg::is_pandas_series() const { return pyobj.is_pandas_series(); }
bool Arg::is_numpy_array()   const { return pyobj.is_numpy_array(); }



//------------------------------------------------------------------------------
// Type conversions
//------------------------------------------------------------------------------

bool        Arg::to_bool_strict()  const { return pyobj.to_bool_strict(*this); }
int32_t     Arg::to_int32_strict() const { return pyobj.to_int32_strict(*this); }
int64_t     Arg::to_int64_strict() const { return pyobj.to_int64_strict(*this); }
size_t      Arg::to_size_t()       const { return pyobj.to_size_t(*this); }
double      Arg::to_double()       const { return pyobj.to_double(*this); }
py::olist   Arg::to_pylist()       const { return pyobj.to_pylist(*this); }
py::odict   Arg::to_pydict()       const { return pyobj.to_pydict(*this); }
py::rdict   Arg::to_rdict()        const { return pyobj.to_rdict(*this); }
py::otuple  Arg::to_pytuple()      const { return pyobj.to_pytuple(*this); }
std::string Arg::to_string()       const { return pyobj.to_string(*this); }
strvec      Arg::to_stringlist()   const { return pyobj.to_stringlist(*this); }
SType       Arg::to_stype()        const { return pyobj.to_stype(*this); }
SType       Arg::to_stype(const error_manager& em) const { return pyobj.to_stype(em); }
DataTable*  Arg::to_frame()        const { return pyobj.to_frame(*this); }



//------------------------------------------------------------------------------
// Error messages
//------------------------------------------------------------------------------

Error Arg::error_not_list(PyObject* src) const {
  return TypeError() << name() << " should be a list or tuple, instead got "
      << Py_TYPE(src);
}

Error Arg::error_not_stype(PyObject* src) const {
  return TypeError() << name() << " cannot be converted into an stype: " << src;
}

Error Arg::error_not_boolean(PyObject* src) const {
  return TypeError() << name() << " should be a boolean, instead got "
      <<Py_TYPE(src);
}

Error Arg::error_not_integer(PyObject* src) const {
  return TypeError() << name() << " should be an integer, instead got "
      <<Py_TYPE(src);
}

Error Arg::error_int_negative(PyObject* src) const {
  return ValueError() << name() << " cannot be negative: " << src;
}

Error Arg::error_not_double(PyObject* src) const {
  return TypeError() << name() << " should be a float, instead got "
      <<Py_TYPE(src);
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
