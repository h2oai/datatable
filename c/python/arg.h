//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#ifndef dt_PYTHON_ARG_h
#define dt_PYTHON_ARG_h
#include <string>     // std::string
#include <Python.h>

namespace py {

class PKArgs;


class Arg {
  private:
    size_t pos;
    PKArgs* parent;
    PyObject* pyobj;
    mutable std::string cached_name;

  public:
    Arg();
    void init(size_t i, PKArgs* args);
    void set(PyObject* value);

    //---- Type checks -----------------
    bool is_undefined() const;
    bool is_none() const;
    bool is_int() const;
    bool is_float() const;
    bool is_list() const;
    bool is_tuple() const;
    bool is_dict() const;
    bool is_string() const;

    // ?
    PyObject* obj() { return pyobj; }
    void print() const;

    operator int32_t() const;
    operator int64_t() const;

  private:
    const std::string& name() const;
};


}  // namespace py
#endif
