//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#ifndef dt_PYTHON_ARGS_h
#define dt_PYTHON_ARGS_h
#include <unordered_map>   // std::unordered_map
#include <vector>          // std::vector
#include <Python.h>
#include "python/arg.h"
#include "utils/exceptions.h"

namespace py {



//------------------------------------------------------------------------------
// Args
//------------------------------------------------------------------------------

/**
 * Helper class for ExtType: it encapsulates arguments passed to a function
 * and helps to verify / parse them. This is the base class for a family of
 * args-related functions, each designed to handle different calling
 * convention.
 */
class Args {
  private:
    const char* cls_name;
    const char* fun_name;
    mutable const char* full_name;

  public:
    Args();
    virtual ~Args();

    //---- API for ExtType<T> ----------
    void set_class_name(const char* name);
    void set_function_name(const char* name);
    virtual void bind(PyObject* _args, PyObject* _kwds) = 0;

  protected:
    // Each Args describes a certain function or method in a class.
    // This will return the name of that function/method, in the
    // form "foo()" or "Class.foo()".
    const char* get_name() const;
};



//------------------------------------------------------------------------------
// NoArgs
//------------------------------------------------------------------------------

class NoArgs : public Args {
  public:
    void bind(PyObject* _args, PyObject* _kwds) override;
};



//------------------------------------------------------------------------------
// PKArgs
//------------------------------------------------------------------------------

class PKArgs : public Args {
  private:
    const size_t n_posonly_args;
    const size_t n_pos_kwd_args;
    const size_t n_kwdonly_args;
    const size_t n_args;
    const bool   has_varargs;
    const bool   has_varkwds;
    const size_t : 48;
    const std::vector<const char*> arg_names;

    // Runtime arguments
    std::vector<Arg> bound_args;
    std::unordered_map<PyObject*, size_t> kwd_map;

  public:
    PKArgs(size_t npo, size_t npk, size_t nko, bool vargs, bool vkwds,
           std::initializer_list<const char*> _names);

    void bind(PyObject* _args, PyObject* _kws) override;

    /**
     * Returns the name of argument `i`, which will usually be in one of the
     * following forms (depending on whether the argument is positional or
     * named):
     *    "First argument in Class.method()"
     *    "5th argument in Class.method()"
     *    "Argument `foo` in Class.method()"
     * This name can also be obtained as
     *    args[i].name()
     */
    std::string make_arg_name(size_t i) const;

    //---- User API --------------------
    const Arg& operator[](size_t i) const;

    template <typename T> T get(size_t i) const;
    template <typename T> T get(size_t i, T default_value) const;

  private:
    size_t _find_kwd(PyObject* kwd);
};



//------------------------------------------------------------------------------
// templates
//------------------------------------------------------------------------------

template <typename T>
T PKArgs::get(size_t i) const {
  if (bound_args[i].is_undefined()) {
    throw TypeError() << "Argument `" << arg_names[i] << "` is missing";
  }
  return static_cast<T>(bound_args[i]);
}

template <typename T>
T PKArgs::get(size_t i, T default_value) const {
  return bound_args[i].is_undefined()
          ? default_value
          : static_cast<T>(bound_args[i]);
}



}  // namespace py

#endif
