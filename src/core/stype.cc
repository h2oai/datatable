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
#include "python/int.h"
#include "python/obj.h"
#include "python/python.h"
#include "python/tuple.h"
#include "stype.h"
#include "utils/assert.h"
namespace dt {


//------------------------------------------------------------------------------
// Generic stype properties
//------------------------------------------------------------------------------

SType common_stype(SType stype1, SType stype2) {
  // Note: we may need additional logic in the future
  return std::max(stype1, stype2);
}




//------------------------------------------------------------------------------
// Work with Python stype enum objects
//------------------------------------------------------------------------------

static PyObject* Py_Stype_Objects[STYPES_COUNT];  // initialized as nullptrs
static PyTypeObject* Py_Stype = nullptr;


static void _init_py_stype(SType stype) {
  int i = static_cast<int>(stype);
  Py_Stype_Objects[i] = py::robj(reinterpret_cast<PyObject*>(Py_Stype))
                        .call({ py::oint(i) })
                        .release();
}

void init_py_stype_objs(PyObject* stype_enum) {
  Py_Stype = reinterpret_cast<PyTypeObject*>(stype_enum);
  Py_INCREF(stype_enum);

  _init_py_stype(SType::VOID);
  _init_py_stype(SType::BOOL);
  _init_py_stype(SType::INT8);
  _init_py_stype(SType::INT16);
  _init_py_stype(SType::INT32);
  _init_py_stype(SType::INT64);
  _init_py_stype(SType::FLOAT32);
  _init_py_stype(SType::FLOAT64);
  _init_py_stype(SType::STR32);
  _init_py_stype(SType::STR64);
  _init_py_stype(SType::DATE64);
  _init_py_stype(SType::TIME32);
  _init_py_stype(SType::DATE32);
  _init_py_stype(SType::DATE16);
  _init_py_stype(SType::OBJ);
}


bool is_stype_object(PyObject* v) {
  return Py_TYPE(v) == Py_Stype;
}


int stype_from_pyobject(PyObject* s) {
  xassert(s);
  for (size_t i = 0; i < STYPES_COUNT; ++i) {
    if (Py_Stype_Objects[i] == s) return static_cast<int>(i);
  }
  return -1;
}




}

#include "types.h"
py::oobj info::py_stype() const {
  return py::oobj(dt::Py_Stype_Objects[stype]);
}
