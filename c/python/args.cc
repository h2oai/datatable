//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#include "python/args.h"
#include <cstring>             // std::strrchr
#include "utils/assert.h"

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

const char* Args::get_name() const {
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
// PKArgs
//------------------------------------------------------------------------------

PKArgs::PKArgs(
    size_t npo, size_t npk, size_t nko, bool vargs, bool vkwds,
    std::initializer_list<const char*> _names
  )
  : n_posonly_args(npo),
    n_pos_kwd_args(npk),
    n_kwdonly_args(nko),
    n_args(npo + npk + nko),
    has_varargs(vargs),
    has_varkwds(vkwds),
    arg_names(_names)
{
  xassert(n_args == arg_names.size());
  if (has_varargs) xassert(n_pos_kwd_args == 0);
  bound_args.resize(n_args);
  for (size_t i = 0; i < n_args; ++i) {
    bound_args[i].init(i, this);
  }
}


void PKArgs::bind(PyObject* _args, PyObject* _kwds)
{
  size_t nargs = _args? static_cast<size_t>(Py_SIZE(_args)) : 0;
  // size_t nkwds = _kwds? static_cast<size_t>(PyDict_Size(_kwds)) : 0;

  size_t i = 0;
  for (; i < nargs; ++i) {
    bound_args[i].set(PyTuple_GET_ITEM(_args, i));
  }
  for (; i < n_args; ++i) {
    bound_args[i].set(nullptr);
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
      bound_args[ikey].set(value);
    }
  }

}


std::string PKArgs::make_arg_name(size_t i) const {
  std::string res;
  if (i < n_posonly_args) {
    res = (i == 0)? "First" :
          (i == 1)? "Second" :
          (i == 2)? "Third" :
          std::to_string(i + 1) + "th";
    res += " argument";
  } else {
    res = std::string("Argument `") + arg_names[i] + '`';
  }
  res += std::string(" in ") + get_name();
  return res;
}


size_t PKArgs::_find_kwd(PyObject* kwd) {
  try {
    return kwd_map.at(kwd);
  } catch (const std::out_of_range&) {
    size_t i = 0;
    for (const char* name : arg_names) {
      if (PyUnicode_CompareWithASCIIString(kwd, name) == 0) {
        // We store the reference to `kwd` and increase its refcount, making
        // `kwd` effectively immortal. Usually this shouldn't matter -- strings
        // that are used as keyword are normally static in the program. However
        // in the extremely rare cases when the keywords are dynamic, we don't
        // want a PyObject* to be gc-ed, then re-created with the content of
        // another string while `kwd_map` would still map it to the original
        // index...
        Py_INCREF(kwd);
        kwd_map[kwd] = i;
        return i;
      }
      ++i;
    }
  }
  throw TypeError() << get_name() << " got an unexpected keyword argument `"
      << PyUnicode_AsUTF8(kwd)<< '`';
}



const Arg& PKArgs::operator[](size_t i) const {
  return bound_args[i];
}




}  // namespace py
