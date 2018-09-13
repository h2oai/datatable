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
    n_all_args(npo + npk + nko),
    has_varargs(vargs),
    has_varkwds(vkwds),
    arg_names(_names),
    n_varkwds(0)
{
  xassert(n_all_args == arg_names.size());
  if (has_varargs) xassert(n_pos_kwd_args == 0);
  bound_args.resize(n_all_args);
  for (size_t i = 0; i < n_all_args; ++i) {
    bound_args[i].init(i, this);
  }
}


void PKArgs::bind(PyObject* _args, PyObject* _kwds)
{
  size_t nargs = _args? static_cast<size_t>(Py_SIZE(_args)) : 0;
  // size_t nkwds = _kwds? static_cast<size_t>(PyDict_Size(_kwds)) : 0;

  size_t max_pos_args = n_posonly_args + n_pos_kwd_args;
  size_t n_bound_args = std::min(nargs, max_pos_args);
  n_varargs = nargs - n_bound_args;
  if (n_varargs && !has_varargs) {
    auto err = TypeError() << get_name();
    if (max_pos_args == 0) {
      err << " takes no positional arguments";
    } else if (max_pos_args == 1) {
      err << " takes only one positional argument";
    } else {
      err << " takes at most " << max_pos_args << " positional arguments";
    }
    err << ", but " << nargs << (nargs == 1? " was given" : " were given");
    throw err;
  }

  size_t i = 0;
  for (; i < n_bound_args; ++i) {
    bound_args[i].set(PyTuple_GET_ITEM(_args, i));
  }
  for (; i < n_all_args; ++i) {
    bound_args[i].set(nullptr);
  }

  if (_kwds) {
    PyObject* key, *value;
    Py_ssize_t pos = 0;
    n_varkwds = 0;
    while (PyDict_Next(_kwds, &pos, &key, &value)) {
      size_t ikey = _find_kwd(key);
      if (ikey == size_t(-1)) {
        n_varkwds++;
        if (has_varkwds) continue;
        throw TypeError() << get_name() << " got an unexpected keyword "
          "argument `" << PyUnicode_AsUTF8(key) << '`';
      }
      if (ikey < nargs) {
        throw TypeError() << get_name() << " got multiple values for argument `"
            << PyUnicode_AsUTF8(key) << '`';
      }
      bound_args[ikey].set(value);
    }
  }
  kwds_dict = _kwds;
  args_tuple = _args;
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
  return size_t(-1);
}



const Arg& PKArgs::operator[](size_t i) const {
  return bound_args[i];
}

size_t PKArgs::num_vararg_args() const noexcept {
  return n_varargs;
}

size_t PKArgs::num_varkwd_args() const noexcept {
  return n_varkwds;
}

VarKwdsIterable PKArgs::varkwds() const noexcept {
  return VarKwdsIterable(*this);
}

VarArgsIterable PKArgs::varargs() const noexcept {
  return VarArgsIterable(*this);
}



//------------------------------------------------------------------------------
// Helper iterator classes
//------------------------------------------------------------------------------

VarKwdsIterable::VarKwdsIterable(const PKArgs& args)
    : parent(args) {}

VarKwdsIterator VarKwdsIterable::begin() const {
  return VarKwdsIterator(parent, 0);
}

VarKwdsIterator VarKwdsIterable::end() const {
  return VarKwdsIterator(parent, -1);
}


VarArgsIterable::VarArgsIterable(const PKArgs& args)
    : parent(args) {}

VarArgsIterator VarArgsIterable::begin() const {
  size_t i0 = parent.n_posonly_args;
  return VarArgsIterator(parent, i0);
}

VarArgsIterator VarArgsIterable::end() const {
  size_t i1 = parent.n_posonly_args + parent.n_varargs;
  return VarArgsIterator(parent, i1);
}



VarKwdsIterator::VarKwdsIterator(const PKArgs& args, Py_ssize_t i0)
    : parent(args), pos(i0), curr_value(std::string(), py::obj(nullptr))
{
  advance();
}

VarKwdsIterator& VarKwdsIterator::operator++() {
  advance();
  return *this;
}

VarKwdsIterator::value_type VarKwdsIterator::operator*() const {
  return curr_value;
}

bool VarKwdsIterator::operator==(const VarKwdsIterator& other) const {
  return (pos == other.pos);
}

bool VarKwdsIterator::operator!=(const VarKwdsIterator& other) const {
  return (pos != other.pos);
}

void VarKwdsIterator::advance() {
  if (pos == -1) return;
  PyObject *key, *value;
  while (PyDict_Next(parent.kwds_dict, &pos, &key, &value)) {
    if (parent.kwd_map.count(key) == 0) {
      curr_value = value_type(py::obj(key).to_string(),
                              py::obj(value));
      return;
    }
  }
  pos = -1;
}



VarArgsIterator::VarArgsIterator(const PKArgs& args, size_t i0)
    : parent(args), pos(i0) {}

VarArgsIterator& VarArgsIterator::operator++() {
  ++pos;
  return *this;
}

py::obj VarArgsIterator::operator*() const {
  PyObject* tup = parent.args_tuple;
  return py::obj(PyTuple_GET_ITEM(tup, static_cast<Py_ssize_t>(pos)));
}

bool VarArgsIterator::operator==(const VarArgsIterator& other) const {
  return (pos == other.pos);
}

bool VarArgsIterator::operator!=(const VarArgsIterator& other) const {
  return (pos != other.pos);
}


}  // namespace py
