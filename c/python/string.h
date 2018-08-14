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
  protected:
    PyObject* obj;

  public:
    string();
    string(PyObject*);
    string(const string&);
    friend void swap(string& first, string& second) noexcept;
};



class ostring : public string {
  public:
    ostring();
    ostring(const std::string& s);
    ostring(const char* str);
    ostring(const char* str, size_t len);
    ostring(PyObject*);
    ostring(const ostring&);
    ostring(ostring&&);
    ~ostring();
    friend oobj::oobj(ostring&&);
};


}  // namespace py

#endif
