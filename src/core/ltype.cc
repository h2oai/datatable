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
#include "_dt.h"
#include "ltype.h"
#include "python/int.h"
#include "python/obj.h"
#include "python/python.h"
#include "python/tuple.h"
#include "utils/assert.h"
namespace dt {

static PyObject* Py_Ltype_Objects[LTYPES_COUNT];  // initialized as nullptrs
static PyTypeObject* Py_Ltype = nullptr;



const char* ltype_name(LType lt) {
  switch (lt) {
    case LType::MU:       return "void";
    case LType::BOOL:     return "bool";
    case LType::INT:      return "int";
    case LType::REAL:     return "float";
    case LType::STRING:   return "str";
    case LType::DATETIME: return "time";
    case LType::DURATION: return "duration";
    case LType::OBJECT:   return "object";
    default:              return "---";  // LCOV_EXCL_LINE
  }
}




//------------------------------------------------------------------------------
// Interoperate with Python ltype objects
//------------------------------------------------------------------------------

static void _init_py_ltype(LType ltype) {
  int i = static_cast<int>(ltype);
  Py_Ltype_Objects[i] = py::robj(reinterpret_cast<PyObject*>(Py_Ltype))
                        .call({ py::oint(i) })
                        .release();
}

void init_py_ltype_objs(PyObject* ltype_enum) {
  Py_Ltype = reinterpret_cast<PyTypeObject*>(ltype_enum);
  Py_INCREF(ltype_enum);

  _init_py_ltype(LType::MU);
  _init_py_ltype(LType::BOOL);
  _init_py_ltype(LType::INT);
  _init_py_ltype(LType::REAL);
  _init_py_ltype(LType::STRING);
  _init_py_ltype(LType::DATETIME);
  _init_py_ltype(LType::OBJECT);
  _init_py_ltype(LType::INVALID);
}


int ltype_from_pyobject(PyObject* lt) {
  xassert(lt);
  PyObject* res = PyObject_CallFunction(
      reinterpret_cast<PyObject*>(Py_Ltype), "O", lt
  );
  if (res == nullptr) {
    PyErr_Clear();
    return -1;
  }
  int32_t value = py::robj(res).get_attr("value").to_int32();
  return value;
}


py::oobj ltype_to_pyobj(LType ltype) {
  return py::oobj(Py_Ltype_Objects[static_cast<size_t>(ltype)]);
}


bool is_ltype_object(PyObject* v) {
  return Py_TYPE(v) == Py_Ltype;
}


bool ltype_is_numeric(LType ltype) {
  return ltype == LType::MU ||
         ltype == LType::BOOL ||
         ltype == LType::INT ||
         ltype == LType::REAL;
}




}  // namespace dt
