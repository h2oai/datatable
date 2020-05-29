//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#ifndef dt_TYPES_H
#define dt_TYPES_H
#include <algorithm> // std::min
#include <cstring>   // std::strncmp
#include <cmath>     // isnan
#include <limits>    // std::numeric_limits
#include <string>    // std::string
#include "python/python.h"
#include "stype.h"
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

  bool operator<(const CString& other) const {
    if (ch == other.ch && size == other.size) // Short circuit test
      return false;
    size_t cmpsize = static_cast<size_t>(std::min(size, other.size));
    int comparison = std::strncmp(ch, other.ch, cmpsize);
    if (comparison != 0)
      return comparison < 0;
    return size < other.size;
  }

  bool operator>(const CString& other) const {
    if (ch == other.ch && size == other.size) // Short circuit test
      return false;
    size_t cmpsize = static_cast<size_t>(std::min(size, other.size));
    int comparison = std::strncmp(ch, other.ch, cmpsize);
    if (comparison != 0)
      return comparison > 0;
    return size > other.size;
  }

  bool operator<=(const CString& other) const {
    return !(*this > other);
  }

  bool operator>=(const CString& other) const {
    return !(*this < other);
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
namespace py { class oobj; }

class info {
  private:
    uint8_t stype;

  public:
    info(dt::SType s);
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

inline bool is_numeric(dt::SType st) {
  return static_cast<uint8_t>(st) <= static_cast<uint8_t>(dt::SType::FLOAT64);
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
dt::SType common_stype(dt::SType stype1, dt::SType stype2);


#endif
