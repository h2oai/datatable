//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#include "python/args.h"
#include "utils/exceptions.h"

namespace py {


//------------------------------------------------------------------------------
// Args
//------------------------------------------------------------------------------

Args::Args() : fn_name(nullptr) {}

Args::Args(const char* name) : fn_name(name) {}

Args::~Args() {}

void Args::set_name(const char* name) { fn_name = name; }

bool Args::has(size_t) const { return false; }

bool Args::has(const char*) const { return false; }

Arg Args::get(size_t) const { throw AssertionError(); }

Arg Args::get(const char*) const { throw AssertionError(); }



//------------------------------------------------------------------------------
// NoArgs
//------------------------------------------------------------------------------

void NoArgs::bind(PyObject* _args, PyObject* _kwds) {
  if ((_args && Py_SIZE(_args)) || (_kwds && PyDict_Size(_kwds))) {
    throw TypeError() << fn_name << "() accepts no arguments";
  }
}



//------------------------------------------------------------------------------
// PosAndKwdArgs
//------------------------------------------------------------------------------

PosAndKwdArgs::PosAndKwdArgs(
    const char* name, size_t np, std::initializer_list<const char*> ki)
    : Args(name), n_posonly_args(np), n_kwdonly_args(0), kwd_names(ki),
      args(nullptr), kwds(nullptr) {}


void PosAndKwdArgs::bind(PyObject* _args, PyObject* _kws) {
  args = _args;
  kwds = _kws;

  size_t nargs = static_cast<size_t>(Py_SIZE(args));

  if (nargs < n_posonly_args) {
    size_t nmiss = n_posonly_args - nargs;
    throw TypeError() << fn_name << "() is missing " << nmiss
        << " required positional argument" << (nmiss > 1 ? "s" : "");
  }

}


}  // namespace py
