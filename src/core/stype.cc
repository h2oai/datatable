//------------------------------------------------------------------------------
// Copyright 2018-2021 H2O.ai
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
#include "ltype.h"
#include "python/int.h"
#include "python/obj.h"
#include "python/python.h"
#include "python/tuple.h"
#include "stype.h"
#include "utils/assert.h"
namespace dt {

static PyObject* Py_Stype_Objects[STYPES_COUNT];  // initialized as nullptrs
static PyTypeObject* Py_Stype = nullptr;



//------------------------------------------------------------------------------
// Generic stype properties
//------------------------------------------------------------------------------

SType common_stype(SType stype1, SType stype2) {
  // Note: we may need additional logic in the future
  return std::max(stype1, stype2);
}


LType stype_to_ltype(SType stype) {
  switch (stype) {
    case SType::VOID   : return LType::MU;
    case SType::BOOL   : return LType::BOOL;
    case SType::INT8   : return LType::INT;
    case SType::INT16  : return LType::INT;
    case SType::INT32  : return LType::INT;
    case SType::INT64  : return LType::INT;
    case SType::FLOAT32: return LType::REAL;
    case SType::FLOAT64: return LType::REAL;
    case SType::STR32  : return LType::STRING;
    case SType::STR64  : return LType::STRING;
    case SType::TIME64 : return LType::DATETIME;
    case SType::DATE32 : return LType::DATETIME;
    case SType::OBJ    : return LType::OBJECT;
    default            : return LType::INVALID;
  }
}


const char* stype_name(SType stype) {
  switch (stype) {
    case SType::VOID   : return "void";
    case SType::BOOL   : return "bool8";
    case SType::INT8   : return "int8";
    case SType::INT16  : return "int16";
    case SType::INT32  : return "int32";
    case SType::INT64  : return "int64";
    case SType::FLOAT32: return "float32";
    case SType::FLOAT64: return "float64";
    case SType::STR32  : return "str32";
    case SType::STR64  : return "str64";
    case SType::ARR32  : return "arr32";
    case SType::ARR64  : return "arr64";
    case SType::TIME64 : return "time64";
    case SType::DATE32 : return "date32";
    case SType::OBJ    : return "obj64";
    case SType::AUTO   : return "auto";
    default            : return "unknown";
  }
}


size_t stype_elemsize(SType stype) {
  switch (stype) {
    case SType::VOID   : return 0;
    case SType::BOOL   : return sizeof(element_t<SType::BOOL>);
    case SType::INT8   : return sizeof(element_t<SType::INT8>);
    case SType::INT16  : return sizeof(element_t<SType::INT16>);
    case SType::INT32  : return sizeof(element_t<SType::INT32>);
    case SType::INT64  : return sizeof(element_t<SType::INT64>);
    case SType::FLOAT32: return sizeof(element_t<SType::FLOAT32>);
    case SType::FLOAT64: return sizeof(element_t<SType::FLOAT64>);
    case SType::STR32  : return sizeof(element_t<SType::STR32>);
    case SType::STR64  : return sizeof(element_t<SType::STR64>);
    case SType::ARR32  : return sizeof(element_t<SType::ARR32>);
    case SType::ARR64  : return sizeof(element_t<SType::ARR64>);
    case SType::TIME64 : return sizeof(element_t<SType::TIME64>);
    case SType::DATE32 : return sizeof(element_t<SType::DATE32>);
    case SType::OBJ    : return sizeof(element_t<SType::OBJ>);
    default            : return 0;
  }
}


bool stype_is_fixed_width(SType stype) {
  return !stype_is_variable_width(stype);
}

bool stype_is_variable_width(SType stype) {
  return (stype == SType::STR32 || stype == SType::STR64);
}

py::oobj stype_to_pyobj(SType stype) {
  auto s = static_cast<size_t>(stype);
  xassert(s < STYPES_COUNT);
  return py::oobj(Py_Stype_Objects[s]);
}




//------------------------------------------------------------------------------
// Interoperate with Python stype objects
//------------------------------------------------------------------------------

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
  _init_py_stype(SType::TIME64);
  _init_py_stype(SType::DATE32);
  _init_py_stype(SType::OBJ);
}


bool is_stype_object(PyObject* v) {
  return Py_TYPE(v) == Py_Stype;
}


int stype_from_pyobject(PyObject* s) {
  xassert(s);
  PyObject* res = PyObject_CallFunction(
      reinterpret_cast<PyObject*>(Py_Stype), "O", s
  );
  if (res == nullptr) {
    PyErr_Clear();
    return -1;
  }
  int32_t value = py::robj(res).get_attr("value").to_int32();
  return value;
}




}  // namespace dt
