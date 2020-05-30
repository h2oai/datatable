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
#include <algorithm>
#include "python/obj.h"
#include "python/python.h"
#include "stype.h"
extern PyTypeObject* py_stype;
extern PyTypeObject* py_ltype;

namespace dt {

static PyObject* py_stype_objs[STYPES_COUNT];




SType common_stype(SType stype1, SType stype2) {
  // Note: we may need additional logic in the future
  return std::max(stype1, stype2);
}


void init_py_stype_objs(PyObject* stype_enum) {
  Py_INCREF(stype_enum);
  py_stype = reinterpret_cast<PyTypeObject*>(stype_enum);
  for (size_t i = 0; i < STYPES_COUNT; ++i) {
    // The call may raise an exception -- that's ok
    py_stype_objs[i] = PyObject_CallFunction(stype_enum, "i", i);
    if (py_stype_objs[i] == nullptr) {
      PyErr_Clear();
      py_stype_objs[i] = Py_None;
    }
  }
}





}

#include "types.h"
py::oobj info::py_stype() const {
  return py::oobj(dt::py_stype_objs[stype]);
}
