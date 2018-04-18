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
#include "utils/pyobj.h"



/**
 * C++ wrapper around PyUnicode_Object (python `str` object).
 */
class PyyString {
  private:
    PyObject* obj;

  public:
    PyyString();
    PyyString(const std::string& s);
    PyyString(const char* str);
    PyyString(const char* str, size_t len);
    PyyString(PyObject*);
    PyyString(const PyyString&);
    PyyString(PyyString&&);
    ~PyyString();

    operator PyObj() const &;
    operator PyObj() &&;

    friend void swap(PyyString& first, PyyString& second) noexcept;
};



#endif
