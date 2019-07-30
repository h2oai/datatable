//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#ifndef dt_PYTHON_STRING_h
#define dt_PYTHON_STRING_h
#include <string>
#include "python/obj.h"

namespace py {


/**
 * Wrapper around a Python string.
 */
class ostring : public oobj {
  public:
    ostring();
    ostring(const std::string&);
    ostring(const CString&);
    ostring(const char* str);
    ostring(const char* str, size_t len);
    ostring(const ostring&);
    ostring(ostring&&);
    ostring& operator=(const ostring&);
    ostring& operator=(ostring&&);

  private:
    ostring(PyObject*);
    static ostring from_new_reference(PyObject*);
    friend class _obj;
};


}  // namespace py

#endif
