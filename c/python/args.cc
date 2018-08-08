//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#include "python/args.h"
#include <cstring>             // std::strrchr
#include "utils/exceptions.h"

namespace py {


//------------------------------------------------------------------------------
// Args
//------------------------------------------------------------------------------

Args::Args() : cls_name(nullptr), fun_name(nullptr), full_name(nullptr) {}

Args::~Args() {
  delete[] full_name;
}

void Args::set_class_name(const char* name) {
  const char* p = std::strrchr(name, '.');
  cls_name = p? p + 1 : name;
}

void Args::set_function_name(const char* name) {
  fun_name = name;
}

const char* Args::get_name() {
  if (full_name) return full_name;
  size_t len1 = cls_name? std::strlen(cls_name) : 0;
  size_t len2 = fun_name? std::strlen(fun_name) : 0;
  size_t len_total = (len1? len1 + 1 : 0) + (len2? len2 : 1) + 2 + 1;
  char* ptr = new char[len_total];
  full_name = ptr;
  if (len1) {
    std::strncpy(ptr, cls_name, len1);
    ptr += len1;
    *ptr++ = '.';
  }
  if (len2) {
    std::strncpy(ptr, fun_name, len2);
    ptr += len2;
  } else {
    *ptr++ = '?';
  }
  *ptr++ = '(';
  *ptr++ = ')';
  *ptr = '\0';
  return full_name;
}



//------------------------------------------------------------------------------
// NoArgs
//------------------------------------------------------------------------------

void NoArgs::bind(PyObject* _args, PyObject* _kwds) {
  if ((_args && Py_SIZE(_args)) || (_kwds && PyDict_Size(_kwds))) {
    throw TypeError() << get_name() << " accepts no arguments";
  }
}



//------------------------------------------------------------------------------
// PosAndKwdArgs
//------------------------------------------------------------------------------

PosAndKwdArgs::PosAndKwdArgs(
    size_t npo, size_t nko, std::initializer_list<const char*> ki
  ) : kwd_names(ki)
{
  n_posonly_args = npo;
  n_kwdonly_args = nko;
  n_total_args = npo + kwd_names.size();
  bound_args.resize(n_total_args);
}


void PosAndKwdArgs::bind(PyObject* _args, PyObject* _kwds)
{
  size_t nargs = _args? static_cast<size_t>(Py_SIZE(_args)) : 0;
  // size_t nkwds = _kwds? static_cast<size_t>(PyDict_Size(_kwds)) : 0;

  if (nargs < n_posonly_args) {
    size_t nmiss = n_posonly_args - nargs;
    throw TypeError() << get_name() << " is missing " << nmiss
        << " required positional argument" << (nmiss > 1 ? "s" : "");
  }
  size_t i = 0;
  for (; i < nargs; ++i) {
    bound_args[i] = PyTuple_GET_ITEM(_args, i);
  }
  for (; i < n_total_args; ++i) {
    bound_args[i] = nullptr;
  }

  if (_kwds) {
    PyObject* key, *value;
    Py_ssize_t pos = 0;
    while (PyDict_Next(_kwds, &pos, &key, &value)) {
      size_t ikey = _find_kwd(key);
      if (ikey < nargs) {
        throw TypeError() << get_name() << " got multiple values for argument `"
            << PyUnicode_AsUTF8(key) << '`';
      }
      bound_args[ikey] = value;
    }
  }

  // for (i = nargs; i < n_required_args; ++i) {
  //   if (bound_args[i] == nullptr) {
  //     throw TypeError() << get_name() << " missing a required keyword-only "
  //         "argument `" << kwd_names[i] << "`";
  //   }
  // }
}


size_t PosAndKwdArgs::_find_kwd(PyObject* kwd) {
  try {
    return kwd_map.at(kwd);
  } catch (const std::out_of_range&) {
    size_t i = 0;
    for (const char* name : kwd_names) {
      if (PyUnicode_CompareWithASCIIString(kwd, name) == 0) {
        kwd_map[kwd] = i;
        return i;
      }
      ++i;
    }
  }
  throw TypeError() << get_name() << " got an unexpected keyword argument `"
      << PyUnicode_AsUTF8(kwd)<< '`';
}


bool PosAndKwdArgs::has(size_t i) const {
  return bound_args[i] != nullptr;
}

bool PosAndKwdArgs::has(const char*) const { return false; }

Arg PosAndKwdArgs::get(size_t) const { throw AssertionError(); }

Arg PosAndKwdArgs::get(const char*) const { throw AssertionError(); }


}  // namespace py
