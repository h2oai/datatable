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
#include "python/obj.h"
#include "utils/assert.h"
#include "utils/misc.h"
#include "types.h"
#include "stype.h"

static PyObject* py_ltype_objs[DT_LTYPES_COUNT];
PyTypeObject* py_ltype;
PyTypeObject* py_stype;



//==============================================================================
// Static asserts
//==============================================================================

static_assert(INTPTR_MAX == INT64_MAX,
              "Only 64 bit platforms are supported.");

static_assert(sizeof(void*) == 8, "Expected size(void*) to be 8 bytes");
static_assert(sizeof(void*) == sizeof(size_t),
              "size(size_t) != size(void*)");
static_assert(sizeof(void*) == sizeof(int64_t),
              "size(int64_t) != size(void*)");

static_assert(sizeof(int8_t) == 1, "int8_t should be 1-byte");
static_assert(sizeof(int16_t) == 2, "int16_t should be 2-byte");
static_assert(sizeof(int32_t) == 4, "int32_t should be 4-byte");
static_assert(sizeof(int64_t) == 8, "int64_t should be 8-byte");
static_assert(sizeof(float) == 4, "float should be 4-byte");
static_assert(sizeof(double) == 8, "double should be 8-byte");
static_assert(sizeof(char) == sizeof(unsigned char), "char != uchar");
static_assert(sizeof(char) == 1, "sizeof(char) != 1");

static_assert(sizeof(LType) == 1, "LType does not fit in a byte");
static_assert(sizeof(dt::SType) == 1, "SType does not fit in a byte");

static_assert(static_cast<unsigned>(-1) - static_cast<unsigned>(-3) == 2,
              "Unsigned arithmetics check");
static_assert(3u - (-1u) == 4u, "Unsigned arithmetics check");
static_assert(-1u == 0xFFFFFFFFu, "Unsigned arithmetics check");

static_assert(sizeof(int64_t) == sizeof(Py_ssize_t),
              "int64_t and Py_ssize_t should refer to the same type");



//==============================================================================
// Initialize auxiliary data structures
//==============================================================================
/**
 * Information about `SType`s, for programmatic access.
 *
 * code:
 *     0-terminated 3-character string representing the stype in a form easily
 *     understandable by humans.
 *
 * elemsize:
 *     number of storage bytes per element (for fixed-size types), so that the
 *     amount of memory required to store a column with `n` rows would be
 *     `n * elemsize`. For variable-size types, this field gives the minimal
 *     storage size per element.
 *
 * varwidth:
 *     flag indicating whether the field is variable-width. If this is false,
 *     then the column is a plain array of elements, each of `elemsize` bytes.
 *     If this flag is true, then the field has more complex layout and
 *     specialized logic to handle that layout.
 *
 * ltype:
 *     which :enum:`LType` corresponds to this SType.
 *
 */
struct STypeInfo {
  size_t      elemsize;
  const char* name;
  LType       ltype;
  bool        varwidth;
  size_t : 48;  // padding
};


static STypeInfo stype_info[dt::STYPES_COUNT];

void init_types(void)
{
  #define STI(T, code2, name, csize, vw, ltype) \
      stype_info[int(T)] = STypeInfo{csize, name, ltype, vw}
  STI(dt::SType::VOID,    "v0", "void",    0, 0, LType::MU);
  STI(dt::SType::BOOL,    "b1", "bool8",   1, 0, LType::BOOL);
  STI(dt::SType::INT8,    "i1", "int8",    1, 0, LType::INT);
  STI(dt::SType::INT16,   "i2", "int16",   2, 0, LType::INT);
  STI(dt::SType::INT32,   "i4", "int32",   4, 0, LType::INT);
  STI(dt::SType::INT64,   "i8", "int64",   8, 0, LType::INT);
  STI(dt::SType::FLOAT32, "r4", "float32", 4, 0, LType::REAL);
  STI(dt::SType::FLOAT64, "r8", "float64", 8, 0, LType::REAL);
  STI(dt::SType::STR32,   "s4", "str32",   4, 1, LType::STRING);
  STI(dt::SType::STR64,   "s8", "str64",   8, 1, LType::STRING);
  STI(dt::SType::DATE64,  "t8", "date64",  8, 0, LType::DATETIME);
  STI(dt::SType::TIME32,  "T4", "time32",  4, 0, LType::DATETIME);
  STI(dt::SType::DATE32,  "t4", "date32",  4, 0, LType::DATETIME);
  STI(dt::SType::DATE16,  "t2", "date16",  2, 0, LType::DATETIME);
  STI(dt::SType::OBJ,     "o8", "obj64",   8, 0, LType::OBJECT);
  #undef STI

  //---- More static asserts -------------------------------------------------
  #if DT_DEBUG
    // This checks validity of a cast used in reader_parsers.cc
    // Cannot use `char ch` as a loop variable here because `127 + 1 == -128`.
    for (int i = -128; i <= 127; i++) {
      char ch = static_cast<char>(i);
      xassert((ch >= '0' && ch <= '9') ==
              (static_cast<uint_fast8_t>(ch - '0') < 10));
    }
  #endif
}






void init_py_ltype_objs(PyObject* ltype_enum)
{
  Py_INCREF(ltype_enum);
  py_ltype = reinterpret_cast<PyTypeObject*>(ltype_enum);
  for (size_t i = 0; i < DT_LTYPES_COUNT; ++i) {
    // The call may raise an exception -- that's ok
    py_ltype_objs[i] = PyObject_CallFunction(ltype_enum, "i", i);
    if (py_ltype_objs[i] == nullptr) {
      PyErr_Clear();
      py_ltype_objs[i] = Py_None;
    }
  }
}


//------------------------------------------------------------------------------

info::info(dt::SType s) {
  stype = static_cast<uint8_t>(s);
}

const char* info::name() const {
  return stype_info[stype].name;
}

size_t info::elemsize() const {
  return stype_info[stype].elemsize;
}

bool info::is_varwidth() const {
  return stype_info[stype].varwidth;
}

LType info::ltype() const {
  return stype_info[stype].ltype;
}

const char* info::ltype_name(LType lt) {
  switch (lt) {
    case LType::MU:       return "void";
    case LType::BOOL:     return "boolean";
    case LType::INT:      return "integer";
    case LType::REAL:     return "float";
    case LType::STRING:   return "string";
    case LType::DATETIME: return "time";
    case LType::DURATION: return "duration";
    case LType::OBJECT:   return "object";
  }
  throw RuntimeError() << "Unknown ltype " << int(lt); // LCOV_EXCL_LINE
}

const char* info::ltype_name() const {
  LType lt = ltype();
  return ltype_name(lt);
}

py::oobj info::py_ltype() const {
  return py::oobj(py_ltype_objs[static_cast<uint8_t>(ltype())]);
}
