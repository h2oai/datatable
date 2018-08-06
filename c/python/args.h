//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#ifndef dt_PYTHON_ARGS_h
#define dt_PYTHON_ARGS_h
#include <vector>
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
  protected:
    const char* fn_name;

  public:
    Args();
    Args(const char* name);
    virtual ~Args();

    virtual void bind(PyObject* _args, PyObject* _kwds) = 0;

    virtual bool has(size_t i) const;
    virtual bool has(const char* name) const;
    virtual Arg get(size_t i) const;
    virtual Arg get(const char* name) const;
};



//------------------------------------------------------------------------------
// NoArgs
//------------------------------------------------------------------------------

class NoArgs : public Args {
  public:
    using Args::Args;

    void bind(PyObject* _args, PyObject* _kwds) override;
};



//------------------------------------------------------------------------------
// PosAndKwdArgs
//------------------------------------------------------------------------------

class PosAndKwdArgs : public Args {
  private:
    // All references are borrowed
    size_t n_posonly_args;
    size_t n_kwdonly_args;
    std::vector<const char*> kwd_names;
    std::vector<PyObject*> defaults;
    PyObject* args;
    PyObject* kwds;

  public:
    PosAndKwdArgs(const char*, size_t, std::initializer_list<const char*>);

    void bind(PyObject* _args, PyObject* _kws) override;
};


}  // namespace py

#endif
