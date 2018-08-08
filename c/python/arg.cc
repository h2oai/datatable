//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#include "python/arg.h"
#include "utils/exceptions.h"

namespace py {


Arg::Arg()
  : name(nullptr), pyobj(nullptr), deflt(nullptr) {}


void Arg::set(PyObject* value) {
  pyobj = value;
}


bool Arg::is_present() const {
  return (pyobj != nullptr);
}


const Arg& Arg::get() const {
  if (pyobj == nullptr) {
    if (deflt == nullptr) {
      throw TypeError() << "Missing argument `" << name << "`";
    }
    pyobj = deflt;
  }
  return *this;
}


}  // namespace py
