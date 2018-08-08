//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#ifndef dt_PYTHON_ARG_h
#define dt_PYTHON_ARG_h
#include <Python.h>

namespace py {


class Arg {
  private:
    const char* name;
    mutable PyObject* pyobj;
    PyObject* deflt;

  public:
    Arg();

    void set(PyObject* value);

    bool is_present() const;
    const Arg& get() const;

    PyObject* obj() { return pyobj; }
};


}  // namespace py
#endif
