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
#ifndef dt_STYPE_h
#define dt_STYPE_h
#include <cmath>     // std::isnan
#include <limits>    // std::numeric_limits
#include "_dt.h"     // size_t, uint8_t, PyObject
#include "cstring.h"
#include "python/obj.h"
#include "python/python.h"
#include "types/type.h"
namespace dt {


/**
  * "Storage" type of a data column.
  *
  * These storage types are in 1-to-many correspondence with the
  * logical types. That is, a single logical type may have multiple
  * storage types, but not the other way around.
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
  ARR32   = 13,
  ARR64   = 14,
  DATE32  = 17,
  TIME64  = 18,
  OBJ     = 21,
  CAT8    = 22,
  CAT16   = 23,
  CAT32   = 24,
  AUTO    = 30,
  INVALID = 31,
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
template <> struct _elt<SType::DATE32>  { using t = int32_t; };
template <> struct _elt<SType::TIME64>  { using t = int64_t; };
template <> struct _elt<SType::STR32>   { using t = uint32_t; };
template <> struct _elt<SType::STR64>   { using t = uint64_t; };
template <> struct _elt<SType::ARR32>   { using t = uint32_t; };
template <> struct _elt<SType::ARR64>   { using t = uint64_t; };
template <> struct _elt<SType::OBJ>     { using t = PyObject*; };
template <> struct _elt<SType::CAT8>    { using t = uint8_t; };
template <> struct _elt<SType::CAT16>   { using t = uint16_t; };
template <> struct _elt<SType::CAT32>   { using t = uint32_t; };

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
inline bool compatible_type(SType stype) {
  return Type::from_stype(stype).can_be_read_as<T>();
}




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
