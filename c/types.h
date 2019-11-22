//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#ifndef dt_TYPES_H
#define dt_TYPES_H
#include <cstring>   // std::strncmp
#include <cmath>     // isnan
#include <limits>    // std::numeric_limits
#include <string>    // std::string
#include "python/python.h"
using size_t = std::size_t;


struct CString {
  const char* ch;
  int64_t size;  // TODO: convert into size_t

  CString() : ch(nullptr), size(0) {}
  CString(const char* ptr, int64_t sz) : ch(ptr), size(sz) {}
  CString(const std::string& str)
    : ch(str.data()), size(static_cast<int64_t>(str.size())) {}
  CString(const CString&) = default;
  CString& operator=(const CString&) = default;
  CString& operator=(const std::string& str) {
    ch = str.data();
    size = static_cast<int64_t>(str.size());
    return *this;
  }
  operator bool() const { return ch != nullptr; }
  bool isna() const { return ch == nullptr; }

  bool operator==(const CString& other) const {
    return (size == other.size) &&
           ((ch == other.ch) ||  // This ensures NAs are properly handled too
            (std::strncmp(ch, other.ch, static_cast<size_t>(size)) == 0));
  }

  std::string to_string() const {
    return std::string(ch, static_cast<size_t>(size));
  }
};


//==============================================================================

/**
 * "Logical" type of a data column.
 *
 * Logical type is supposed to match the user's notion of a column type. For
 * example logical "LType::INT" type corresponds to the mathematical set of
 * integers, and thus reflects the usual notion of what the "integer" *is*.
 *
 * Each logical type has multiple underlying "storage" types, that describe
 * how the type is actually stored in memory. For example, LType::INT can be
 * stored as an 8-, 16-, 32- or a 64-bit integer. All "storage" types within
 * a single logical type should be freely interchangeable: operators or
 * functions that accept certain logical type should be able to work with any
 * its storage subtype.
 *
 * Different logical types may or may not be interchangeable, depending on the
 * use case. For example, most binary operators would promote boolean ->
 * integer -> real; however some operators / functions may not. For example,
 * bit shift operators require integer (or boolean) arguments.
 *
 *
 * LType::MU
 *     special "marker" type for a column that has unknown type. For example,
 *     this can be used to indicate that the system should autodetect the
 *     column's type from the data. This type has no storage types.
 *
 * LType::BOOL
 *     column for storing boolean (0/1) values. Right now we only allow to
 *     store booleans as 1-byte signed chars. In most arithmetic expressions
 *     booleans are automatically promoted to integers (or reals) if needed.
 *
 * LType::INT
 *     integer values, equivalent of ℤ in mathematics. We support multiple
 *     storage sizes for integers: from 8 bits to 64 bits, but do not allow
 *     arbitrary-length integers. In most expressions integers will be
 *     automatically promoted to reals if needed.
 *
 * LType::REAL
 *     real values, equivalent of ℝ in mathematics. We store these in either
 *     fixed- or floating-point formats.
 *
 * LType::STRING
 *     all strings are encoded in UTF-8. We allow either variable-width strings
 *     or fixed-width.
 *
 * LType::DATETIME
 * LType::DURATION
 *
 * LType::OBJECT
 *     column for storing all other values of arbitrary (possibly heterogeneous)
 *     types. Each element is a `PyObject*`. Missing values are `Py_None`s.
 *
 */
enum class LType : uint8_t {
  MU       = 0,
  BOOL     = 1,
  INT      = 2,
  REAL     = 3,
  STRING   = 4,
  DATETIME = 5,
  DURATION = 6,
  OBJECT   = 7,
};

constexpr size_t DT_LTYPES_COUNT = static_cast<size_t>(LType::OBJECT) + 1;



//==============================================================================

/**
 * "Storage" type of a data column.
 *
 * These storage types are in 1-to-many correspondence with the logical types.
 * That is, a single logical type may have multiple storage types, but not the
 * other way around.
 *
 * SType::VOID
 *     "Fake" type, column with this stype is in an invalid state. This type may
 *     be used internally to indicate that a column is being constructed.
 *
 * -----------------------------------------------------------------------------
 *
 * SType::BOOL
 *     elem: int8_t (1 byte)
 *     NA:   -128
 *     A boolean with True = 1, False = 0. All other values are invalid.
 *
 * -----------------------------------------------------------------------------
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
 *     An integer in the range -9223372036854775807 .. 9223372036854775807.
 *
 * -----------------------------------------------------------------------------
 *
 * SType::FLOAT32
 *     elem: float (4 bytes)
 *     NA:   0x7F8007A2
 *     Floating-point real number, corresponding to C's `float` (IEEE 754). We
 *     designate a specific NAN payload to mean the NA value; whereas all other
 *     numbers starting with 0x7F8 or 0xFF8 should be treated as actual NANs (or
 *     infinities).
 *
 * SType::FLOAT64
 *     elem: double (8 bytes)
 *     NA:   0x7FF00000000007A2
 *     Floating-point real number, corresponding to C's `double` (IEEE 754).
 *
 * SType::DEC16
 *     elem: int16_t (2 bytes)
 *     NA:   -2**15 = -32768
 *     The fixed-point real number (aka decimal); the scale variable in the meta
 *     indicates the number of digits after the decimal point. For example,
 *     number 7.11 can be stored as integer 711 with scale = 2.
 *     Note that this is different from IEEE 754 "decimal" format, since we
 *     include scale into the meta information of the column, rather than into
 *     each value. Thus, all values will have common scale, which greatly
 *     simplifies their use.
 *
 * SType::DEC32
 *     elem: int32_t (4 bytes)
 *     NA:   -2**31
 *     Fixed-point real number stored as a 32-bit integer.
 *
 * SType::DEC64
 *     elem: int64_t (8 bytes)
 *     NA:   -2**63
 *     Fixed-point real number stored as a 64-bit integer.
 *
 * -----------------------------------------------------------------------------
 *
 * SType::STR32
 *     elem: uint32_t (4 bytes) + unsigned char[]
 *     NA:   (1 << 31) mask
 *     Variable-width strings. The data consists of 2 buffers:
 *       1) string data. All non-NA strings are UTF-8 encoded and placed e
 *          nd-to-end.
 *       2) offsets. This is an array of uint32_t's representing offsets of
 *          each string in buffer 1). This array has nrows + 1 elements, with
 *          first element being 0, second element containing the end offset of
 *          the first string, ..., until the last element which stores the end
 *          offset of the last string. NA strings are encoded as the previous
 *          offset + NA mask (which is 1<<31).
 *     Thus, i-th string is NA if its highest bit is set, or otherwise it's a
 *     valid string whose starting offset is `start(i) = off(i-1) & ~(1<<31)`,
 *     ending offset is `end(i) = off(i)`, and `len(i) = end(i) - start(i)`.
 *
 *     For example, a column with 4 values `[NA, "hello", "", NA]` will be
 *     encoded as a string buffer of size 5 = (0 + 5 + 0 + 0) and offsets
 *     buffer containing 5 uint32_t's:
 *         strbuf  = [h e l l o]
 *         offsets = [0, 1<<31, 5, 5, 5|(1<<31)]
 *
 * SType::STR64
 *     elem: uint64_t (8 bytes) + unsigned char[]
 *     NA:   (1 << 63) mask
 *     Variable-width strings: same as SType::STR32 but use 64-bit
 *     offsets.
 *
 * SType::FSTR
 *     elem: unsigned char[] (n bytes)
 *     NA:   0xFF 0xFF ... 0xFF
 *     Fixed-width strings, similar to "CHAR(n)" in SQL. These strings have
 *     constant width `n` and are therefore stored as `char[n]` arrays. They are
 *     *not* null-terminated, however strings that are shorter than `n` in width
 *     will be \0-padded. The width `n` > 0 is given in the metadata. String
 *     data is encoded in UTF-8. The NA value is encoded as a sequence of 0xFF
 *     bytes, which is not a valid UTF-8 string.
 *
 * SType::CAT8
 *     elem: uint8_t (1 byte)
 *     NA:   255
 *     String column stored as a categorical variable (aka "factor" or "enum").
 *     This type is suitable for columns with low cardinality, i.e. having no
 *     more than 255 distinct string values.
 *     The data is stored in a buffer with 3 regions: first comes the string
 *     data, padded to a multiple of 8 bytes; then comes the array of int32
 *     offsets within the data buffer, these offsets describe the boundaries of
 *     individual strings within the data section (this array is also padded
 *     to a multiple of 8 bytes); finally comes the array of categorical
 *     indices. Meta information contains offsets of the second and the third
 *     sections. The layout of the first 2 sections is exactly the same as
 *     that of the SType::STR32 type.
 *
 * SType::CAT16
 *     elem: uint16_t (2 bytes)
 *     NA:   65535
 *     Strings stored as a categorical variable with no more than 65535 distinct
 *     levels. The layout is exactly the same as that of CAT8, only
 *     the last section uses 2 bytes per element instead of just 1 byte.
 *
 * SType::CAT32
 *     elem: uint32_t (4 bytes)
 *     NA:   2**32-1
 *     Strings stored as a categorical variable with no more than 2**32-1
 *     distinct levels. (The combined size of all categorical strings may not
 *     exceed 2**32 too). The layout is same as that of CAT8, only
 *     the last section uses 4 bytes per element instead of just 1 byte.
 *
 *
 * -----------------------------------------------------------------------------
 *
 * SType::DATE64
 *     elem: int64_t (8 bytes)
 *     NA:   -2**63
 *     Timestamp, stored as the number of microseconds since 0000-03-01. The
 *     allowed time range is ≈290,000 years around the epoch. The time is
 *     assumed to be in UTC, and does not allow specifying a time zone.
 *
 * SType::DATE32
 *     elem: int32_t (4 bytes)
 *     NA:   -2**31
 *     Date only: the number of days since 0000-03-01. The allowed time range
 *     is ≈245,000 years.
 *
 * SType::DATE16
 *     elem: int16_t (2 bytes)
 *     NA:   -2**15
 *     Year+month only: the number of months since 0000-03-01. The allowed time
 *     range is up to year 2730.
 *     This type is specifically designed for business applications. It allows
 *     adding/subtraction in monthly/yearly intervals (other datetime types do
 *     not allow that since months/years have uneven lengths).
 *
 * SType::TIME32
 *     elem: int32_t (4 bytes)
 *     NA:   -2**31
 *     Time only: the number of milliseconds since midnight. The allowed time
 *     range is ≈24 days.
 *
 *
 * -----------------------------------------------------------------------------
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
  DEC16   = 8,
  DEC32   = 9,
  DEC64   = 10,
  STR32   = 11,
  STR64   = 12,
  FSTR    = 13,
  CAT8    = 14,
  CAT16   = 15,
  CAT32   = 16,
  DATE64  = 17,
  TIME32  = 18,
  DATE32  = 19,
  DATE16  = 20,
  OBJ     = 21,

  INVALID = 22,
};

constexpr size_t DT_STYPES_COUNT = static_cast<size_t>(SType::INVALID);



//==============================================================================
namespace py { class oobj; }

class info {
  private:
    uint8_t stype;

  public:
    info(SType s);
    const char* name() const;
    static const char* ltype_name(LType);
    const char* ltype_name() const;
    size_t elemsize() const;
    bool is_varwidth() const;
    LType ltype() const;
    py::oobj py_stype() const;
    py::oobj py_ltype() const;
};



//==============================================================================

inline bool is_numeric(SType st) {
  return static_cast<uint8_t>(st) <= static_cast<uint8_t>(SType::FLOAT64);
}


/**
 * NA constants
 */

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
 * Template function that returns the appropriate NA_XX value based on the
 * type of `T`. Returns NULL if `T` is incompatible.
 */
template <typename T>
           inline T        GETNA() { return NULL;  }
template<> inline int8_t   GETNA() { return NA_I1; }
template<> inline int16_t  GETNA() { return NA_I2; }
template<> inline int32_t  GETNA() { return NA_I4; }
template<> inline int64_t  GETNA() { return NA_I8; }
template<> inline uint32_t GETNA() { return NA_S4; }
template<> inline uint64_t GETNA() { return NA_S8; }
template<> inline float    GETNA() { return NA_F4; }
template<> inline double   GETNA() { return NA_F8; }
template<> inline PyObject* GETNA() { return Py_None; }
template<> inline CString  GETNA() { return CString(); }

/**
 * ISNA function
 * Template function that uses the appropriate ISNA_XX macro/function based
 * on the argument type. Returns true if type is invalid.
 */
template <typename T>
           inline bool ISNA(T)          { return true;       }
template<> inline bool ISNA(int8_t x)   { return x == NA_I1; }
template<> inline bool ISNA(int16_t x)  { return x == NA_I2; }
template<> inline bool ISNA(int32_t x)  { return x == NA_I4; }
template<> inline bool ISNA(int64_t x)  { return x == NA_I8; }
template<> inline bool ISNA(uint32_t x) { return (x & NA_S4); }
template<> inline bool ISNA(uint64_t x) { return (x & NA_S8); }
template<> inline bool ISNA(float x)    { return std::isnan(x); }
template<> inline bool ISNA(double x)   { return std::isnan(x); }
template<> inline bool ISNA(PyObject* x) { return x == Py_None; }

/**
 * Similar to ISNA<T>(x) template, except it returns true only for integer-
 * valued types which are NAs.
 * In certain circumstances this could be useful to avoid unnecessary NA checks,
 * since <float> and <double> are represented as NaNs, and using them in any
 * arithmetic expressions will automatically create NaN values as the result
 * without the need for explicit checks.
 */
template<typename T> inline bool IsIntNA(T) { return false; }
template<> inline bool IsIntNA(int8_t x)    { return x == NA_I1; }
template<> inline bool IsIntNA(int16_t x)   { return x == NA_I2; }
template<> inline bool IsIntNA(int32_t x)   { return x == NA_I4; }
template<> inline bool IsIntNA(int64_t x)   { return x == NA_I8; }

//==============================================================================

// Initializer function
void init_types(void);
void init_py_stype_objs(PyObject* stype_enum);
void init_py_ltype_objs(PyObject* ltype_enum);

extern PyTypeObject* py_stype;
extern PyTypeObject* py_ltype;

int stype_from_pyobject(PyObject* s);
SType common_stype(SType stype1, SType stype2);


#endif
