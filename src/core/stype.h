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
#include <cmath>     // std::isnan
#include <limits>    // std::numeric_limits
#include "_dt.h"     // size_t, uint8_t, PyObject
#include "cstring.h"
#include "python/obj.h"
#include "python/python.h"
namespace dt {


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
 * -------------------------------------------------------------------
 *
 * SType::AUTO
 *     Special stype that indicates a type that must be automatically
 *     detected. A column with this stype cannot be created.
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

  AUTO    = 22,
  INVALID = 23,
};

constexpr size_t STYPES_COUNT = static_cast<size_t>(SType::INVALID);

// Can stypes be used in a 64-bit bitmask?
// XXX: which functionality relies on this assumption?
static_assert(STYPES_COUNT <= 64, "Too many stypes");



//------------------------------------------------------------------------------
// Templates
//------------------------------------------------------------------------------

/**
  * Helper template to convert between an stype and the C type
  * of the underlying column element:
  *
  * element_t<stype>
  *   resolves to the type of the element that is in the main data
  *   buffer of a column with the given stype.
  *
  * TODO: element_t<SType::BOOL> should be changed to `bool`, once
  *       NA flags are stored as a separate bitmask.
  */
template <SType> struct _elt {};
template <> struct _elt<SType::VOID>    { using t = void; };
template <> struct _elt<SType::BOOL>    { using t = int8_t; };
template <> struct _elt<SType::INT8>    { using t = int8_t; };
template <> struct _elt<SType::INT16>   { using t = int16_t; };
template <> struct _elt<SType::INT32>   { using t = int32_t; };
template <> struct _elt<SType::INT64>   { using t = int64_t; };
template <> struct _elt<SType::FLOAT32> { using t = float; };
template <> struct _elt<SType::FLOAT64> { using t = double; };
template <> struct _elt<SType::DATE64>  { using t = int64_t; };
template <> struct _elt<SType::DATE32>  { using t = int32_t; };
template <> struct _elt<SType::DATE16>  { using t = int16_t; };
template <> struct _elt<SType::TIME32>  { using t = int32_t; };
template <> struct _elt<SType::STR32>   { using t = uint32_t; };
template <> struct _elt<SType::STR64>   { using t = uint64_t; };
template <> struct _elt<SType::OBJ>     { using t = PyObject*; };

template <SType s>
using element_t = typename _elt<s>::t;


/**
  * The type of variable that can be used to read the elements from
  * a column with the given stype. Example:
  *
  *     read_t<stype> value;
  *     bool isvalid = col.get_element(i, &value);
  *
  */
template <SType s> struct _rdt { using t = element_t<s>; };
template <> struct _rdt<SType::STR32> { using t = CString; };
template <> struct _rdt<SType::STR64> { using t = CString; };
template <> struct _rdt<SType::OBJ>   { using t = py::robj; };

template <SType s>
using read_t = typename _rdt<s>::t;


/**
  * Approximate inverse of `read_t<SType>`: given a type T, returns
  * the "most typical" SType that represents type T.
  */
template <typename T>
            inline constexpr SType _sfr()            { return SType::VOID; }
template <> inline constexpr SType _sfr<bool>()      { return SType::BOOL; }
template <> inline constexpr SType _sfr<int8_t>()    { return SType::INT8; }
template <> inline constexpr SType _sfr<int16_t>()   { return SType::INT16; }
template <> inline constexpr SType _sfr<int32_t>()   { return SType::INT32; }
template <> inline constexpr SType _sfr<int64_t>()   { return SType::INT64; }
template <> inline constexpr SType _sfr<float>()     { return SType::FLOAT32; }
template <> inline constexpr SType _sfr<double>()    { return SType::FLOAT64; }
template <> inline constexpr SType _sfr<CString>()   { return SType::STR32; }
template <> inline constexpr SType _sfr<PyObject*>() { return SType::OBJ; }
template <> inline constexpr SType _sfr<py::robj>()  { return SType::OBJ; }

template <typename T>
static constexpr SType stype_from = _sfr<T>();


/**
  * ref_t<T> returns a "reference type" for T. If T is a primitive
  * C type such as int, float, etc, then this returns the type itself,
  * otherwise it returns `const T&`.
  *
  * This is useful in circumstances where you need to create a
  * function that will accept one of the `read_t<T>` types as an
  * argument. In such case it is more efficient to pass primitive
  * values by value, while more complicated objects by const-ref.
  */
template <typename T> struct _ref  { using t = T; };
template <> struct _ref<CString>   { using t = const CString&; };
template <> struct _ref<PyObject*> { using t = const PyObject*; };
template <> struct _ref<py::oobj>  { using t = const py::oobj&; };

template <typename T>
using ref_t = typename _ref<T>::t;


/**
  * Function that tests whether the given type T is suitable for
  * reading values from a column of the given stype.
  */
template<typename T>
inline bool compatible_type(SType) { return false; }
template<> inline bool compatible_type<int8_t>(SType s)   { return (s == SType::VOID || s == SType::INT8 || s == SType::BOOL); }
template<> inline bool compatible_type<int16_t>(SType s)  { return (s == SType::INT16); }
template<> inline bool compatible_type<int32_t>(SType s)  { return (s == SType::INT32); }
template<> inline bool compatible_type<int64_t>(SType s)  { return (s == SType::INT64); }
template<> inline bool compatible_type<float>(SType s)    { return (s == SType::FLOAT32); }
template<> inline bool compatible_type<double>(SType s)   { return (s == SType::FLOAT64); }
template<> inline bool compatible_type<CString>(SType s)  { return (s == SType::STR32 || s == SType::STR64); }
template<> inline bool compatible_type<py::oobj>(SType s) { return (s == SType::OBJ); }




//------------------------------------------------------------------------------
// NA templates
//------------------------------------------------------------------------------

// NA constants
constexpr int8_t   NA_I1 = INT8_MIN;
constexpr int16_t  NA_I2 = INT16_MIN;
constexpr int32_t  NA_I4 = INT32_MIN;
constexpr int64_t  NA_I8 = INT64_MIN;
constexpr uint8_t  NA_U1 = UINT8_MAX;
constexpr uint16_t NA_U2 = UINT16_MAX;
constexpr uint32_t NA_U4 = UINT32_MAX;
constexpr uint64_t NA_U8 = UINT64_MAX;
constexpr uint32_t NA_S4 = uint32_t(1) << 31;
constexpr uint64_t NA_S8 = uint64_t(1) << 63;
constexpr float    NA_F4 = std::numeric_limits<float>::quiet_NaN();
constexpr double   NA_F8 = std::numeric_limits<double>::quiet_NaN();

/**
  * GETNA function
  * Template function that returns the appropriate NA_XX value based
  * on the type of `T`. Returns NULL if `T` is incompatible.
  */
template <typename T>
           inline T         GETNA() { return NULL;  }
template<> inline int8_t    GETNA() { return NA_I1; }
template<> inline int16_t   GETNA() { return NA_I2; }
template<> inline int32_t   GETNA() { return NA_I4; }
template<> inline int64_t   GETNA() { return NA_I8; }
template<> inline uint32_t  GETNA() { return NA_S4; }
template<> inline uint64_t  GETNA() { return NA_S8; }
template<> inline float     GETNA() { return NA_F4; }
template<> inline double    GETNA() { return NA_F8; }
template<> inline PyObject* GETNA() { return Py_None; }
template<> inline py::oobj  GETNA() { return py::None(); }
template<> inline CString   GETNA() { return CString(); }

/**
  * ISNA function
  * Template function that uses the appropriate ISNA_XX macro/function
  * based on the argument type. Returns true if type is invalid.
  */
template <typename T>
           inline bool ISNA(T)           { return true;       }
template<> inline bool ISNA(int8_t x)    { return x == NA_I1; }
template<> inline bool ISNA(int16_t x)   { return x == NA_I2; }
template<> inline bool ISNA(int32_t x)   { return x == NA_I4; }
template<> inline bool ISNA(int64_t x)   { return x == NA_I8; }
template<> inline bool ISNA(uint32_t x)  { return (x & NA_S4); }
template<> inline bool ISNA(uint64_t x)  { return (x & NA_S8); }
template<> inline bool ISNA(float x)     { return std::isnan(x); }
template<> inline bool ISNA(double x)    { return std::isnan(x); }
template<> inline bool ISNA(PyObject* x) { return x == Py_None; }

/**
  * Similar to ISNA<T>(x) template, except it returns true only for
  * integer-valued types which are NAs.
  * In certain circumstances this could be useful to avoid unnecessary
  * NA checks, since <float> and <double> are represented as NaNs, and
  * using them in any arithmetic expressions will automatically create
  * NaN values as the result without the need for explicit checks.
  */
template<typename T> inline bool IsIntNA(T) { return false; }
template<> inline bool IsIntNA(int8_t x)    { return x == NA_I1; }
template<> inline bool IsIntNA(int16_t x)   { return x == NA_I2; }
template<> inline bool IsIntNA(int32_t x)   { return x == NA_I4; }
template<> inline bool IsIntNA(int64_t x)   { return x == NA_I8; }




//------------------------------------------------------------------------------
// Helper functions
//------------------------------------------------------------------------------

SType common_stype(SType stype1, SType stype2);

LType       stype_to_ltype(SType);
const char* stype_name(SType);
size_t      stype_elemsize(SType);
bool        stype_is_fixed_width(SType);
bool        stype_is_variable_width(SType);
py::oobj    stype_to_pyobj(SType);


// Used by obj.cc exclusively
bool is_stype_object(PyObject*);
int stype_from_pyobject(PyObject*);

// Invoked once during module initialization
void init_py_stype_objs(PyObject* stype_enum);




}  // namespace dt
#endif
