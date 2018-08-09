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
    size_t n_posonly_args;
    size_t n_pos_kwd_args;
    size_t n_kwdonly_args;
    size_t n_args;
    bool   has_varargs;
    bool   has_varkwds;
    size_t : 48;
    std::vector<const char*> arg_names;

    // Runtime arguments
    std::vector<Arg> bound_args;
    std::unordered_map<PyObject*, size_t> kwd_map;

  public:
    PKArgs(size_t npo, size_t npk, size_t nko, bool vargs, bool vkwds,
           std::initializer_list<const char*> _names);

    void bind(PyObject* _args, PyObject* _kws) override;

    std::string make_arg_name(size_t i) const;

    //---- User API --------------------
    const Arg& operator[](size_t i) const;

    template <typename T> T get(size_t i) const;
    template <typename T> T get(size_t i, T default_value) const;

  private:
    size_t _find_kwd(PyObject* kwd);
};


}  // namespace py

#endif
