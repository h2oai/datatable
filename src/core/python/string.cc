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
#include "cstring.h"
#include "python/python.h"
#include "python/string.h"
#include "utils/exceptions.h"
namespace py {


//------------------------------------------------------------------------------
// Constructors
//------------------------------------------------------------------------------

ostring::ostring() : oobj() {}

ostring::ostring(PyObject* src) : oobj(src) {}

ostring ostring::from_new_reference(PyObject* ref) {
  ostring res;
  res.v = ref;
  return res;
}


ostring::ostring(const char* str, size_t len) {
  Py_ssize_t slen = static_cast<Py_ssize_t>(len);
  v = PyUnicode_FromStringAndSize(str, slen);
  if (!v) throw PyError();
}

ostring::ostring(const std::string& s)
  : ostring(s.data(), s.size()) {}

ostring::ostring(const dt::CString& s)
  : ostring(s.ch, static_cast<size_t>(s.size)) {}

ostring::ostring(const char* str)
  : ostring(str, std::strlen(str)) {}


ostring::ostring(const ostring& other) : oobj(other) {}

ostring::ostring(ostring&& other) : oobj(std::move(other)) {}

ostring& ostring::operator=(const ostring& other) {
  oobj::operator=(other);
  return *this;
}

ostring& ostring::operator=(ostring&& other) {
  oobj::operator=(std::move(other));
  return *this;
}



}  // namespace py
