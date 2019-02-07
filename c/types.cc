//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#include "python/obj.h"
#include "utils/assert.h"
#include "types.h"
#include "utils.h"

static PyObject* py_ltype_objs[DT_LTYPES_COUNT];
static PyObject* py_stype_objs[DT_STYPES_COUNT];
PyTypeObject* py_ltype;
PyTypeObject* py_stype;



//==============================================================================
// Static asserts
//==============================================================================

dt_static_assert(INTPTR_MAX == INT64_MAX,
                 "Only 64 bit platforms are supported.");

dt_static_assert(sizeof(void*) == 8, "Expected size(void*) to be 8 bytes");
dt_static_assert(sizeof(void*) == sizeof(size_t),
                 "size(size_t) != size(void*)");
dt_static_assert(sizeof(void*) == sizeof(int64_t),
                 "size(int64_t) != size(void*)");

dt_static_assert(sizeof(int8_t) == 1, "int8_t should be 1-byte");
dt_static_assert(sizeof(int16_t) == 2, "int16_t should be 2-byte");
dt_static_assert(sizeof(int32_t) == 4, "int32_t should be 4-byte");
dt_static_assert(sizeof(int64_t) == 8, "int64_t should be 8-byte");
dt_static_assert(sizeof(float) == 4, "float should be 4-byte");
dt_static_assert(sizeof(double) == 8, "double should be 8-byte");
dt_static_assert(sizeof(char) == sizeof(unsigned char), "char != uchar");
dt_static_assert(sizeof(char) == 1, "sizeof(char) != 1");
// Used in llvm.py
dt_static_assert(sizeof(long long int) == 8, "llint should be 8-byte");

dt_static_assert(sizeof(LType) == 1, "LType does not fit in a byte");
dt_static_assert(sizeof(SType) == 1, "SType does not fit in a byte");

dt_static_assert((unsigned)(-1) - (unsigned)(-3) == 2,
                 "Unsigned arithmetics check");
dt_static_assert(3u - (-1u) == 4u, "Unsigned arithmetics check");
dt_static_assert(-1u == 0xFFFFFFFFu, "Unsigned arithmetics check");



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


static STypeInfo stype_info[DT_STYPES_COUNT];
static SType stype_upcast_map[DT_STYPES_COUNT][DT_STYPES_COUNT];

void init_types(void)
{
  #define STI(T, code2, name, csize, vw, ltype) \
      stype_info[int(T)] = STypeInfo{csize, name, ltype, vw}
  STI(SType::VOID,    "--", "void",    0, 0, LType::MU);
  STI(SType::BOOL,    "b1", "bool8",   1, 0, LType::BOOL);
  STI(SType::INT8,    "i1", "int8",    1, 0, LType::INT);
  STI(SType::INT16,   "i2", "int16",   2, 0, LType::INT);
  STI(SType::INT32,   "i4", "int32",   4, 0, LType::INT);
  STI(SType::INT64,   "i8", "int64",   8, 0, LType::INT);
  STI(SType::FLOAT32, "r4", "float32", 4, 0, LType::REAL);
  STI(SType::FLOAT64, "r8", "float64", 8, 0, LType::REAL);
  STI(SType::DEC16,   "d2", "dec16",   2, 0, LType::REAL);
  STI(SType::DEC32,   "d4", "dec32",   4, 0, LType::REAL);
  STI(SType::DEC64,   "d8", "dec64",   8, 0, LType::REAL);
  STI(SType::STR32,   "s4", "str32",   4, 1, LType::STRING);
  STI(SType::STR64,   "s8", "str64",   8, 1, LType::STRING);
  STI(SType::FSTR,    "sx", "strfix",  0, 0, LType::STRING);
  STI(SType::CAT8,    "e1", "cat8",    1, 1, LType::STRING);
  STI(SType::CAT16,   "e2", "cat16",   2, 1, LType::STRING);
  STI(SType::CAT32,   "e4", "cat32",   4, 1, LType::STRING);
  STI(SType::DATE64,  "t8", "date64",  8, 0, LType::DATETIME);
  STI(SType::TIME32,  "T4", "time32",  4, 0, LType::DATETIME);
  STI(SType::DATE32,  "t4", "date32",  4, 0, LType::DATETIME);
  STI(SType::DATE16,  "t2", "date16",  2, 0, LType::DATETIME);
  STI(SType::OBJ,     "o8", "obj64",   8, 0, LType::OBJECT);
  #undef STI

  #define UPCAST(stype1, stype2, stypeR)         \
      stype_upcast_map[int(stype1)][int(stype2)] = stypeR; \
      stype_upcast_map[int(stype2)][int(stype1)] = stypeR;

  for (size_t i = 1; i < DT_STYPES_COUNT; i++) {
    SType i_stype = static_cast<SType>(i);
    stype_upcast_map[i][0] = stype_info[i].varwidth? SType::OBJ : i_stype;
    stype_upcast_map[0][i] = stype_info[i].varwidth? SType::OBJ : i_stype;
    for (size_t j = 1; j < DT_STYPES_COUNT; j++) {
      stype_upcast_map[i][j] =
        stype_info[i].varwidth || i != j ? SType::OBJ : i_stype;
    }
  }
  UPCAST(SType::BOOL,  SType::INT8,    SType::INT8)
  UPCAST(SType::BOOL,  SType::INT16,   SType::INT16)
  UPCAST(SType::BOOL,  SType::INT32,   SType::INT32)
  UPCAST(SType::BOOL,  SType::INT64,   SType::INT64)
  UPCAST(SType::BOOL,  SType::FLOAT32, SType::FLOAT32)
  UPCAST(SType::BOOL,  SType::FLOAT64, SType::FLOAT64)
  UPCAST(SType::INT8,  SType::INT16,   SType::INT16)
  UPCAST(SType::INT8,  SType::INT32,   SType::INT32)
  UPCAST(SType::INT8,  SType::INT64,   SType::INT64)
  UPCAST(SType::INT8,  SType::FLOAT32, SType::FLOAT32)
  UPCAST(SType::INT8,  SType::FLOAT64, SType::FLOAT64)
  UPCAST(SType::INT16, SType::INT32,   SType::INT32)
  UPCAST(SType::INT16, SType::INT64,   SType::INT64)
  UPCAST(SType::INT16, SType::FLOAT32, SType::FLOAT32)
  UPCAST(SType::INT16, SType::FLOAT64, SType::FLOAT64)
  UPCAST(SType::INT32, SType::INT64,   SType::INT64)
  UPCAST(SType::INT32, SType::FLOAT32, SType::FLOAT32)
  UPCAST(SType::INT32, SType::FLOAT64, SType::FLOAT64)
  UPCAST(SType::INT64, SType::FLOAT32, SType::FLOAT32)
  UPCAST(SType::INT64, SType::FLOAT64, SType::FLOAT64)
  UPCAST(SType::FLOAT32, SType::FLOAT64, SType::FLOAT64)
  #undef UPCAST
  // In py_datatable.c we use 64-bit mask over stypes
  xassert(DT_STYPES_COUNT <= 64);

  //---- More static asserts -------------------------------------------------
  #ifndef NDEBUG
    // This checks validity of a cast used in reader_parsers.cc
    // Cannot use `char ch` as a loop variable here because `127 + 1 == -128`.
    for (int i = -128; i <= 127; i++) {
      char ch = static_cast<char>(i);
      xassert((ch >= '0' && ch <= '9') ==
              (static_cast<uint_fast8_t>(ch - '0') < 10));
    }
  #endif
}


/**
 * Helper function to convert SType from string representation into the numeric
 * constant. The string `s` must have length 3. If the stype is invalid, this
 * function will return SType::VOID.
 */
SType stype_from_string(const std::string& str)
{
  xassert(str.length() == 3 || str.length() == 2);
  char s0 = str[0], s1 = str[1], s2 = str[2];
  if (s0 == 'i') {
    if (s2 == 'i' || s2 == '\0') {
      if (s1 == '1') return SType::INT8;
      if (s1 == '2') return SType::INT16;
      if (s1 == '4') return SType::INT32;
      if (s1 == '8') return SType::INT64;
    } else if (s2 == 'b') {
      if (s1 == '1') return SType::BOOL;
    } else if (s2 == 'r') {
      if (s1 == '2') return SType::DEC16;
      if (s1 == '4') return SType::DEC32;
      if (s1 == '8') return SType::DEC64;
    } else if (s2 == 's') {
      if (s1 == '4') return SType::STR32;
      if (s1 == '8') return SType::STR64;
    } else if (s2 == 'd') {
      if (s1 == '2') return SType::DATE16;
      if (s1 == '4') return SType::DATE32;
      if (s1 == '8') return SType::DATE64;
    } else if (s2 == 't') {
      if (s1 == '4') return SType::TIME32;
    }
  } else if (s0 == 'r' && s2 == '\0') {
    if (s1 == '4') return SType::FLOAT32;
    if (s1 == '8') return SType::FLOAT64;
  } else if (s0 == 'b' && s2 == '\0') {
    if (s1 == '1' && s2 == '\0') return SType::BOOL;
  } else if (s0 == 'o') {
    if (s1 == '8' && s2 == '\0') return SType::OBJ;
  } else if (s0 == 's') {
    if (s2 == '\0') {
      if (s1 == '4') return SType::STR32;
      if (s1 == '8') return SType::STR64;
      if (s1 == 'x') return SType::FSTR;
    }
  } else if (s0 == 'f') {
    if (s2 == 'r') {
      if (s1 == '4') return SType::FLOAT32;
      if (s1 == '8') return SType::FLOAT64;
    }
  } else if (s0 == 'u') {
    if (s2 == 'e') {
      if (s1 == '1') return SType::CAT8;
      if (s1 == '2') return SType::CAT16;
      if (s1 == '4') return SType::CAT32;
    }
  } else if (s0 == 'c') {
    if (s1 == '#' && s2 == 's') return SType::FSTR;
  } else if (s0 == 'p') {
    if (s1 == '8' && s2 == 'p') return SType::OBJ;
  } else if (s0 == 'd' && s2 == '\0') {
    if (s1 == '2') return SType::DEC16;
    if (s1 == '4') return SType::DEC32;
    if (s1 == '8') return SType::DEC64;
  } else if (s0 == 'e' && s2 == '\0') {
    if (s1 == '1') return SType::CAT8;
    if (s1 == '2') return SType::CAT16;
    if (s1 == '4') return SType::CAT32;
  } else if (s0 == 't' && s2 == '\0') {
    if (s1 == '2') return SType::DATE16;
    if (s1 == '4') return SType::DATE32;
    if (s1 == '8') return SType::DATE64;
  } else if (s0 == 'T' && s1 == '4' && s2 == '\0') {
    return SType::TIME32;
  }
  return SType::VOID;
}


int stype_from_pyobject(PyObject* s) {
  PyObject* res = PyObject_CallFunction(
      reinterpret_cast<PyObject*>(py_stype), "O", s
  );
  if (res == nullptr) {
    PyErr_Clear();
    return -1;
  }
  int32_t value = py::robj(res).get_attr("value").to_int32();
  return value;
}


SType common_stype_for_buffer(SType stype1, SType stype2) {
  return stype_upcast_map[int(stype1)][int(stype2)];
}


void init_py_stype_objs(PyObject* stype_enum) {
  Py_INCREF(stype_enum);
  py_stype = reinterpret_cast<PyTypeObject*>(stype_enum);
  for (size_t i = 0; i < DT_STYPES_COUNT; ++i) {
    // The call may raise an exception -- that's ok
    py_stype_objs[i] = PyObject_CallFunction(stype_enum, "i", i);
    if (py_stype_objs[i] == nullptr) {
      PyErr_Clear();
      py_stype_objs[i] = Py_None;
    }
  }
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

info::info(SType s) {
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

const char* info::ltype_name() const {
  LType lt = ltype();
  switch (lt) {
    case LType::MU:       return "void";
    case LType::BOOL:     return "bool";
    case LType::INT:      return "int";
    case LType::REAL:     return "real";
    case LType::STRING:   return "str";
    case LType::DATETIME: return "time";
    case LType::DURATION: return "duration";
    case LType::OBJECT:   return "obj";
  }
  throw RuntimeError() << "Unknown ltype " << int(lt); // LCOV_EXCL_LINE
}

py::oobj info::py_ltype() const {
  return py::oobj(py_ltype_objs[static_cast<uint8_t>(ltype())]);
}

py::oobj info::py_stype() const {
  return py::oobj(py_stype_objs[stype]);
}
