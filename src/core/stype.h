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
#ifndef dt_STYPE_h
#define dt_STYPE_h


//==============================================================================

/**
 * "Storage" type of a data column.
 *
 * These storage types are in 1-to-many correspondence with the
 * logical types. That is, a single logical type may have multiple
 * storage types, but not the other way around.
 *
 * -------------------------------------------------------------------
 *
 * SType::VOID
 *     This type is used for a column containing only NA values. As
 *     such, it has no element types, and cannot be materialized. It
 *     is also the most flexible of all types, as it can mimic any
 *     other stype.
 *
 * SType::BOOL
 *     elem: int8_t (1 byte)
 *     NA:   -128
 *     A boolean with True = 1, False = 0. All other values are
 *     invalid.
 *
 * -------------------------------------------------------------------
 *
 * SType::INT8
 *     elem: int8_t (1 byte)
 *     NA:   -2**7 = -128
 *     An integer in the range -127 .. 127.
 *
 * SType::INT16
 *     elem: int16_t (2 bytes)
 *     NA:   -2**15 = -32768
 *     An integer in the range -32767 .. 32767.
 *
 * SType::INT32
 *     elem: int32_t (4 bytes)
 *     NA:   -2**31 = -2147483648
 *     An integer in the range -2147483647 .. 2147483647.
 *
 * SType::INT64
 *     elem: int64_t (8 bytes)
 *     NA:   -2**63 = -9223372036854775808
 *     An integer in the range [-M; M], where M = 9223372036854775807.
 *
 * -------------------------------------------------------------------
 *
 * SType::FLOAT32
 *     elem: float (4 bytes)
 *     NA:   nan
 *     Floating-point real number, corresponding to C `float` type
 *     (IEEE 754). We consider any float NaN value to ba an NA.
 *
 * SType::FLOAT64
 *     elem: double (8 bytes)
 *     NA:   nan
 *     Floating-point real number, corresponding to C `double` type
 *     (IEEE 754). Any double NaN value is considered an NA.
 *
 * -------------------------------------------------------------------
 *
 * SType::STR32
 *     elem: uint32_t (4 bytes) + unsigned char[]
 *     NA:   (1 << 31) mask
 *     Variable-width strings. The data consists of 2 buffers:
 *       1) string data. All non-NA strings are UTF-8 encoded and
 *          placed end-to-end.
 *       2) offsets. This is an array of uint32_t's representing
 *          offsets of each string in buffer 1). This array has
 *          nrows + 1 elements, with first element being 0, second
 *          element containing the end offset of the first string,
 *          ..., until the last element which stores the end offset
 *          of the last string. NA strings are encoded as the
 *          previous offset + NA mask (which is 1<<31).
 *     Thus, i-th string is NA if its highest bit is set, or
 *     otherwise it's a valid string whose starting offset is
 *     `start(i) = off(i-1) & ~(1<<31)`, ending offset is
 *     `end(i) = off(i)`, and `len(i) = end(i) - start(i)`.
 *
 *     For example, a column with 4 values `[NA, "hello", "", NA]`
 *     will be encoded as a string buffer of size 5 = (0 + 5 + 0 + 0)
 *     and offsets buffer containing 5 uint32_t's:
 *         strbuf  = [h e l l o]
 *         offsets = [0, 1<<31, 5, 5, 5|(1<<31)]
 *
 * SType::STR64
 *     elem: uint64_t (8 bytes) + unsigned char[]
 *     NA:   (1 << 63) mask
 *     Variable-width strings: same as SType::STR32 but use 64-bit
 *     offsets.
 *
 * -------------------------------------------------------------------
 *
 * SType::DATE64
 *     elem: int64_t (8 bytes)
 *     NA:   -2**63
 *     Timestamp, stored as the number of microseconds since
 *     0000-03-01. The allowed time range is ≈290,000 years around
 *     the epoch. The time is assumed to be in UTC, and does not
 *     allow specifying a time zone.
 *
 * SType::DATE32
 *     elem: int32_t (4 bytes)
 *     NA:   -2**31
 *     Date only: the number of days since 0000-03-01. The allowed
 *     time range is ≈245,000 years.
 *
 * SType::DATE16
 *     elem: int16_t (2 bytes)
 *     NA:   -2**15
 *     Year+month only: the number of months since 0000-03-01. The
 *     allowed time range is up to year 2730.
 *     This type is specifically designed for business applications.
 *     It allows adding/subtraction in monthly/yearly intervals (other
 *     datetime types do not allow that since months/years have uneven
 *     lengths).
 *
 * SType::TIME32
 *     elem: int32_t (4 bytes)
 *     NA:   -2**31
 *     Time only: the number of milliseconds since midnight. The
 *     allowed time range is ≈24 days.
 *
 *
 * -------------------------------------------------------------------
 *
 * SType::OBJ
 *     elem: PyObject*
 *     NA:   Py_None
 *
 */
enum class SType : uint8_t {
  VOID    = 0,
  BOOL    = 1,
  INT8    = 2,
  INT16   = 3,
  INT32   = 4,
  INT64   = 5,
  FLOAT32 = 6,
  FLOAT64 = 7,
  STR32   = 11,
  STR64   = 12,
  DATE64  = 17,
  TIME32  = 18,
  DATE32  = 19,
  DATE16  = 20,
  OBJ     = 21,

  INVALID = 22,
};

constexpr size_t DT_STYPES_COUNT = static_cast<size_t>(SType::INVALID);




#endif
