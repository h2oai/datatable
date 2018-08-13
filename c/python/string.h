//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#ifndef dt_PYTHON_STRING_h
#define dt_PYTHON_STRING_h
#include <Python.h>
#include <string>
#include "python/obj.h"

namespace py {


/**
 * C++ wrapper around PyUnicode_Object (python `str` object).
 */
class string {
  private:
    PyObject* obj;

  public:
    string();
    string(const std::string& s);
    string(const char* str);
    string(const char* str, size_t len);
    string(PyObject*);
    string(const string&);
    string(string&&);
    ~string();

    operator py::oobj() &&;
    PyObject* release();

    friend void swap(string& first, string& second) noexcept;
};



}  // namespace py

#endif
