//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#include "utils/assert.h"
#include "types.h"
#include "utils.h"


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

STypeInfo stype_info[DT_STYPES_COUNT];
static SType stype_upcast_map[DT_STYPES_COUNT][DT_STYPES_COUNT];

void init_types(void)
{
  #define STI(T, code, code2, csize, vw, ltype, na) \
      stype_info[int(T)] = (STypeInfo){csize, 0, na, code, code2, ltype, vw}
  STI(SType::VOID,    "---", "--", 0, 0, LT_MU,       NULL);
  STI(SType::BOOL,    "i1b", "b1", 1, 0, LT_BOOLEAN,  &NA_I1);
  STI(SType::INT8,    "i1i", "i1", 1, 0, LT_INTEGER,  &NA_I1);
  STI(SType::INT16,   "i2i", "i2", 2, 0, LT_INTEGER,  &NA_I2);
  STI(SType::INT32,   "i4i", "i4", 4, 0, LT_INTEGER,  &NA_I4);
  STI(SType::INT64,   "i8i", "i8", 8, 0, LT_INTEGER,  &NA_I8);
  STI(SType::FLOAT32, "f4r", "r4", 4, 0, LT_REAL,     &NA_F4);
  STI(SType::FLOAT64, "f8r", "r8", 8, 0, LT_REAL,     &NA_F8);
  STI(SType::DEC16,   "i2r", "d2", 2, 0, LT_REAL,     &NA_I2);
  STI(SType::DEC32,   "i4r", "d4", 4, 0, LT_REAL,     &NA_I4);
  STI(SType::DEC64,   "i8r", "d8", 8, 0, LT_REAL,     &NA_I8);
  STI(SType::STR32,   "i4s", "s4", 4, 1, LT_STRING,   NULL);
  STI(SType::STR64,   "i8s", "s8", 8, 1, LT_STRING,   NULL);
  STI(SType::FSTR,    "c#s", "sx", 0, 0, LT_STRING,   NULL);
  STI(SType::CAT8,    "u1e", "e1", 1, 1, LT_STRING,   &NA_U1);
  STI(SType::CAT16,   "u2e", "e2", 2, 1, LT_STRING,   &NA_U2);
  STI(SType::CAT32,   "u4e", "e4", 4, 1, LT_STRING,   &NA_U4);
  STI(SType::DATE64,  "i8d", "t8", 8, 0, LT_DATETIME, &NA_I8);
  STI(SType::TIME32,  "i4t", "T4", 4, 0, LT_DATETIME, &NA_I4);
  STI(SType::DATE32,  "i4d", "t4", 4, 0, LT_DATETIME, &NA_I4);
  STI(SType::DATE16,  "i2d", "t2", 2, 0, LT_DATETIME, &NA_I2);
  STI(SType::OBJ,     "p8p", "o8", 8, 0, LT_OBJECT,   NULL);
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

    for (size_t i = 0; i < DT_STYPES_COUNT; i++) {
      xassert(static_cast<SType>(i) == stype_from_string(stype_info[i].code));
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
  }
  return SType::VOID;
}


SType common_stype_for_buffer(SType stype1, SType stype2) {
  return stype_upcast_map[int(stype1)][int(stype2)];
}
