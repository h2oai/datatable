//------------------------------------------------------------------------------
// Copyright 2019 H2O.ai
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
#include "expr/funary/pyfn.h"
#include "expr/funary/umaker.h"
#include "expr/funary/umaker_impl.h"
#include "utils/assert.h"
namespace dt {
namespace expr {



//------------------------------------------------------------------------------
// Op::UPLUS (+)
//------------------------------------------------------------------------------

/**
  * Unary operator `+` upcasts each numeric column to INT32, but
  * otherwise keeps unmodified. The operator cannot be applied to
  * string columns.
  */
umaker_ptr resolve_op_uplus(SType stype)
{
  if (stype == SType::BOOL || stype == SType::INT8 || stype == SType::INT16) {
    return umaker_ptr(new umaker_cast(SType::INT32));
  }
  if (stype == SType::VOID || stype == SType::INT32 || stype == SType::INT64 ||
      stype == SType::FLOAT32 || stype == SType::FLOAT64)
  {
    return umaker_ptr(new umaker_copy());
  }
  throw TypeError() << "Cannot apply unary `operator +` to a column with "
                       "stype `" << stype << "`";
}




//------------------------------------------------------------------------------
// Op::UMINUS (-)
//------------------------------------------------------------------------------

template <typename T>
static inline T op_minus(T x) {
  return -x;
}


template <typename T>
static umaker_ptr _uminus(SType uptype = SType::VOID) {
  if (uptype != SType::VOID) assert_compatible_type<T>(uptype);
  return umaker1<T, T>::make(op_minus<T>, uptype, stype_from<T>());
}


/**
  * Unary operator `-` upcasts numeric columns to INT32.
  */
umaker_ptr resolve_op_uminus(SType stype)
{
  switch (stype) {
    case SType::VOID:    return umaker_ptr(new umaker_copy());
    case SType::BOOL:
    case SType::INT8:
    case SType::INT16:   return _uminus<int32_t>(SType::INT32);
    case SType::INT32:   return _uminus<int32_t>();
    case SType::INT64:   return _uminus<int64_t>();
    case SType::FLOAT32: return _uminus<float>();
    case SType::FLOAT64: return _uminus<double>();
    default:
      throw TypeError() << "Cannot apply unary `operator -` to a column with "
                           "stype `" << stype << "`";
  }
}




//------------------------------------------------------------------------------
// Op::UINVERT (~)
//------------------------------------------------------------------------------

template <typename T>
static inline T op_invert(T x) {
  return ~x;
}

static inline int8_t op_invert_bool(int8_t x) {
  return !x;
}


template <typename T>
static umaker_ptr _uinvert() {
  return umaker1<T, T>::make(op_invert<T>, SType::VOID, stype_from<T>());
}


/**
  * Unary operator `~` acts as logical NOT on a boolean column,
  * and as a bitwise inverse on integer columns. Integer promotions
  * are not applied. The operator is not applicable to floating-point
  * or string columns.
  */
umaker_ptr resolve_op_uinvert(SType stype)
{
  switch (stype) {
    case SType::VOID:    return umaker_ptr(new umaker_copy());
    case SType::BOOL:    return umaker1<int8_t, int8_t>::make(op_invert_bool, SType::VOID, SType::BOOL);
    case SType::INT8:    return _uinvert<int8_t>();
    case SType::INT16:   return _uinvert<int16_t>();
    case SType::INT32:   return _uinvert<int32_t>();
    case SType::INT64:   return _uinvert<int64_t>();
    default:
      throw TypeError() << "Cannot apply unary `operator ~` to a column with "
                           "stype `" << stype << "`";
  }
}




//------------------------------------------------------------------------------
// Op::LEN
//------------------------------------------------------------------------------


/*
inline static bool op_str_len_ascii(CString str, bool isvalid, int64_t* out) {
  *out = str.size;
  return isvalid;
}
*/

static bool op_str_len_unicode(const CString& str, bool isvalid, int64_t* out) {
  if (isvalid) {
    int64_t len = 0;
    const uint8_t* ch = reinterpret_cast<const uint8_t*>(str.ch);
    const uint8_t* end = ch + str.size;
    while (ch < end) {
      uint8_t c = *ch;
      ch += (c < 0x80)? 1 :
            ((c & 0xE0) == 0xC0)? 2 :
            ((c & 0xF0) == 0xE0)? 3 :  4;
      len++;
    }
    *out = len;
  }
  return isvalid;
}


static const char* doc_len =
R"(len(s)
--

The length of the string `s`.
)";

py::PKArgs args_len(1, 0, 0, false, false, {"s"}, "len",  doc_len);


umaker_ptr resolve_op_len(SType stype)
{
  if (stype == SType::VOID) return umaker_ptr(new umaker_nacol());
  if (stype == SType::STR32 || stype == SType::STR64) {
    return umaker2<CString, int64_t>::make(op_str_len_unicode,
                                           SType::VOID, SType::INT64);
  }
  throw TypeError() << "Function `len` cannot be applied to a column of "
                       "type `" << stype << "`";
}




}}  // namespace dt::expr
