//------------------------------------------------------------------------------
// Copyright 2018-2020 H2O.ai
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the "Software"),
// to deal in the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense,
// and/or sell copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.
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
    ostring() = default;
    ostring(const ostring&) = default;
    ostring(ostring&&) = default;
    ostring& operator=(const ostring&) = default;
    ostring& operator=(ostring&&) = default;

    ostring(const std::string&);
    ostring(const dt::CString&);
    ostring(const char* str);
    ostring(const char* str, size_t len);

  private:
    explicit ostring(const robj&);  // these constructors used by py::_obj only
    explicit ostring(oobj&&);
    friend class _obj;
};




}  // namespace py
#endif
