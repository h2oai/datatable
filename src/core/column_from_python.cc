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
#include <algorithm>       // std::min
#include <cstdlib>         // std::abs
#include <cstring>         // std::memcpy
#include <limits>          // std::numeric_limits
#include <type_traits>     // std::is_same
#include "column/pysources.h"  // PyList_ColumnImpl, ...
#include "column/range.h"
#include "python/_all.h"
#include "python/list.h"   // py::olist
#include "python/string.h" // py::ostring
#include "utils/exceptions.h"
#include "utils/macros.h"
#include "utils/misc.h"
#include "column.h"
#include "stype.h"
namespace {

// forward-declare
Column _parse_bool(const Column&, size_t);
Column _parse_int8(const Column&, size_t);
Column _parse_int32(const Column&, size_t);
Column _parse_int64(const Column&, size_t);
Column _parse_double(const Column&, size_t);
Column _parse_string(const Column&, size_t);


// Each parser function has the following signature:
//
//     bool (*parserfn)(const Column& inputcol, size_t i0, Column* outputcol);
//
// Here `inputcol` is the input column of type obj64, `i0` is the
// index of the first non-None element, and `outputcol` is the variable
// where the converted column will be stored.
//
// The function will attempt to parse `inputcol` into one of the
// types that it supports, and save the resulting Column into variable
// `outputcol`. The return value is `true` if the parse was successful,
// and `false` otherwise. The parser may also throw an exception if
// it believes no other parser will be able to parse the input (which
// could happen if the input contained values of incompatible types,
// such as `[5, "boo", 3.7]`).

Error _error(size_t i, py::oobj item, const char* expected_type) {
  return TypeError()
      << "Cannot create column: element at index " << i
      << " is of type " << item.typeobj()
      << ", whereas previous elements were " << expected_type;
}



//------------------------------------------------------------------------------
// Individual parsers
//------------------------------------------------------------------------------

/**
  * Parses a column containing numpy ints (or Nones) of a specific
  * precision T. We do not allow numpy ints to be mixed with python
  * ints.
  */
template <typename T>
Column _parse_npint(const Column& inputcol, size_t i0) {
  size_t n = inputcol.nrows();
  Buffer databuf = Buffer::mem(n * sizeof(T));
  auto outptr = static_cast<T*>(databuf.xptr());

  py::oobj item;
  for (size_t i = 0; i < i0; i++) {
    *outptr++ = dt::GETNA<T>();
  }
  for (size_t i = i0; i < n; i++) {
    inputcol.get_element(i, &item);
    if (item.parse_numpy_int(outptr) ||
        item.parse_none(outptr)) {
      outptr++;
    } else {
      throw _error(i, item, sizeof(T)==1? "np.int8" :
                            sizeof(T)==2? "np.int16" :
                            sizeof(T)==4? "np.int32" : "np.int64");
    }
  }
  return Column::new_mbuf_column(n, dt::stype_from<T>, std::move(databuf));
}


/**
  * Attempt to parse `inputcol` as a boolean column. This function will
  * succeed iff all elements in the input column are either bools, or
  * numpy bools, or Nones.
  *
  * An exception is raised if at least one element was already parsed
  * as boolean, but others cannot be.
  */
Column _parse_bool(const Column& inputcol, size_t i0) {
  size_t n = inputcol.nrows();
  Buffer databuf = Buffer::mem(n);
  auto outptr = static_cast<int8_t*>(databuf.xptr());

  py::oobj item;
  for (size_t i = 0; i < i0; i++) {
    *outptr++ = dt::GETNA<int8_t>();
  }
  for (size_t i = i0; i < n; i++) {
    inputcol.get_element(i, &item);
    if (!(item.parse_bool(outptr) ||
          item.parse_numpy_bool(outptr) ||
          item.parse_none(outptr))) {
      throw _error(i, item, "boolean");
    }
    outptr++;
  }
  return Column::new_mbuf_column(n, dt::SType::BOOL, std::move(databuf));
}


/**
  * Parses a column containing only numbers 0 and 1 as INT8. If
  * any integer other than 0 or 1 is encountered, then the entire
  * column will be re-parsed as INT32.
  */
Column _parse_int8(const Column& inputcol, size_t i0) {
  size_t n = inputcol.nrows();
  Buffer databuf = Buffer::mem(n * sizeof(int8_t));
  auto outptr = static_cast<int8_t*>(databuf.xptr());

  py::oobj item;
  for (size_t i = 0; i < i0; i++) {
    *outptr++ = dt::GETNA<int8_t>();
  }
  for (size_t i = i0; i < n; i++) {
    inputcol.get_element(i, &item);
    if (item.parse_01(outptr) || item.parse_none(outptr)) {
      outptr++;
    }
    else if (item.is_int()) {
      return _parse_int32(inputcol, i0);
    }
    else if (item.is_float()) {
      return _parse_double(inputcol, i0);
    }
    else {
      throw _error(i, item, "int8");
    }
  }
  return Column::new_mbuf_column(n, dt::SType::INT8, std::move(databuf));
}


/**
  * Parses a column containing integers (and `None`s). If we encounter
  * an integer that is too large to fit into INT32, the entire column
  * will be reparsed as either INT64 or FLOAT64 depending on whether
  * the "big" integer fits into int64_t or not.
  */
Column _parse_int32(const Column& inputcol, size_t i0) {
  size_t n = inputcol.nrows();
  Buffer databuf = Buffer::mem(n * sizeof(int32_t));
  auto outptr = static_cast<int32_t*>(databuf.xptr());

  py::oobj item;
  for (size_t i = 0; i < i0; i++) {
    *outptr++ = dt::GETNA<int32_t>();
  }
  for (size_t i = i0; i < n; i++) {
    inputcol.get_element(i, &item);
    if (item.parse_int(outptr) ||   // returns false if overflows
        item.parse_none(outptr)) {
      outptr++;
    }
    else if (item.is_int() || item.is_float()) {
      int64_t tmp;
      if (item.parse_int(&tmp)) {
        return _parse_int64(inputcol, i0);
      } else {
        return _parse_double(inputcol, i0);
      }
    }
    else {
      throw _error(i, item, "int32");
    }
  }
  return Column::new_mbuf_column(n, dt::SType::INT32, std::move(databuf));
}


/**
  * Parse a column containing integers (and `None`s) as INT64. This
  * parser will be invoked only if we find some big integers in the
  * list. If during parsing we encounter integers that are too big
  * even for INT64, we will reparse the column as FLOAT64.
  */
Column _parse_int64(const Column& inputcol, size_t i0) {
  size_t n = inputcol.nrows();
  Buffer databuf = Buffer::mem(n * sizeof(int64_t));
  auto outptr = static_cast<int64_t*>(databuf.xptr());

  py::oobj item;
  for (size_t i = 0; i < i0; i++) {
    *outptr++ = dt::GETNA<int64_t>();
  }
  for (size_t i = i0; i < n; i++) {
    inputcol.get_element(i, &item);
    if (item.parse_int(outptr) ||   // returns false if overflows
        item.parse_none(outptr)) {
      outptr++;
    }
    else if (item.is_int() || item.is_float()) {
      return _parse_double(inputcol, i0);
    }
    else {
      throw _error(i, item, "int64");
    }
  }
  return Column::new_mbuf_column(n, dt::SType::INT64, std::move(databuf));
}


Column _parse_double(const Column& inputcol, size_t i0) {
  size_t n = inputcol.nrows();
  Buffer databuf = Buffer::mem(n * sizeof(double));
  auto outptr = static_cast<double*>(databuf.xptr());

  py::oobj item;
  for (size_t i = 0; i < i0; i++) {
    *outptr++ = dt::GETNA<double>();
  }
  for (size_t i = i0; i < n; i++) {
    inputcol.get_element(i, &item);
    if (item.parse_double(outptr) ||
        item.parse_int(outptr) ||   // converts to ±MAX if overflows
        item.parse_none(outptr)) {
      outptr++;
    }
    else {
      throw _error(i, item, "float");
    }
  }
  return Column::new_mbuf_column(n, dt::SType::FLOAT64, std::move(databuf));
}


/**
  * Parses a column containing numpy floats (or Nones) of a specific
  * precision T. We do not allow numpy floats to be mixed with python
  * floats.
  */
template <typename T>
Column _parse_npfloat(const Column& inputcol, size_t i0) {
  size_t n = inputcol.nrows();
  Buffer databuf = Buffer::mem(n * sizeof(T));
  auto outptr = static_cast<T*>(databuf.xptr());

  py::oobj item;
  for (size_t i = 0; i < i0; i++) {
    *outptr++ = dt::GETNA<T>();
  }
  for (size_t i = i0; i < n; i++) {
    inputcol.get_element(i, &item);
    if (item.parse_numpy_float(outptr) ||
        item.parse_none(outptr)) {
      outptr++;
    } else {
      throw _error(i, item, sizeof(T)==4? "np.float32" : "np.float64");
    }
  }
  return Column::new_mbuf_column(n, dt::stype_from<T>, std::move(databuf));
}


/**
  * Parses a column containing string values (including numpy
  * strings).
  * Due to complexities of constructing a string column directly,
  * we merely check that all values in the inputcol are strings, and
  * then cast+materialize that column into str32 type.
  */
Column _parse_string(const Column& inputcol, size_t i0) {
  py::oobj item;
  for (size_t i = i0; i < inputcol.nrows(); i++) {
    inputcol.get_element(i, &item);
    if (!(item.is_string() || item.is_none() || item.is_numpy_str())) {
      throw _error(i, item, "string");
    }
  }
  Column out = inputcol.cast(dt::Type::str32());
  out.materialize();
  return out;
}




//------------------------------------------------------------------------------
// Main
//------------------------------------------------------------------------------

/**
  * Main parser that calls all other parsers.
  */
Column new_parse_column_auto_type(const Column& inputcol) {
  size_t nrows = inputcol.nrows();

  // First, find how many `None`s we have at the start of the list,
  // and whether we should produce a VOID column.
  size_t i0 = 0;
  py::oobj item0;
  for (; i0 < nrows; ++i0) {
    inputcol.get_element(i0, &item0);
    if (!item0.is_none()) break;
  }
  if (i0 == nrows) {  // Also works when `nrows == 0`
    return Column::new_na_column(nrows, dt::SType::VOID);
  }

  // Then, decide which parser to call, based on the type of the
  // first non-None element in the list.
  int npbits;
  if (item0.is_float()) {
    return _parse_double(inputcol, i0);
  }
  else if (item0.is_int()) {
    int value = item0.to_int32();  // Converts to ±MAX on overflow
    if (value == 0 || value == 1) {
      return _parse_int8(inputcol, i0);
    } else {
      return _parse_int32(inputcol, i0);
    }
  }
  else if (item0.is_bool() || item0.is_numpy_bool()) {
    return _parse_bool(inputcol, i0);
  }
  else if (item0.is_string() || item0.is_numpy_str()) {
    return _parse_string(inputcol, i0);
  }
  else if ((npbits = item0.is_numpy_float())) {
    if (npbits == 4) return _parse_npfloat<float>(inputcol, i0);
    if (npbits == 8) return _parse_npfloat<double>(inputcol, i0);
  }
  else if ((npbits = item0.is_numpy_int())) {
    if (npbits == 1) return _parse_npint<int8_t>(inputcol, i0);
    if (npbits == 2) return _parse_npint<int16_t>(inputcol, i0);
    if (npbits == 4) return _parse_npint<int32_t>(inputcol, i0);
    if (npbits == 8) return _parse_npint<int64_t>(inputcol, i0);
  }

  return Column();
  // if (ok) {
  //   xassert(result);
  //   return result;
  // }
  // py::oobj elem0;
  // inputcol.get_element(i0, &elem0);
  // throw TypeError() << "Cannot create column from a python list: element "
  //     "at index " << i0 << " has type " << elem0.typeobj();
}



}  // namespace
//==============================================================================

/**
  * parse_as_X(inputcol, mbuf, i0)
  *   A family of functions for converting an SType::OBJ input column
  *   into one of the primitive types <X>, if possible. The converted
  *   values will be written into the provided buffer `mbuf`, which
  *   will be automatically reallocated to proper size.
  *
  *   Index `i0` indicates that elements before this index are known
  *   to be convertible, whereas the elements starting from `i0` do
  *   not carry such guarantee. This is a hint variable.
  *
  *   The return value is the index of the first entry that failed to
  *   be converted. If all entries convert successfully, this will be
  *   equal to `inputcol.nrows()`.
  */
template <typename T>
static size_t parse_as_X(const Column& inputcol, Buffer& mbuf, size_t i0,
                         bool(*f)(const py::oobj&, T*))
{
  size_t nrows = inputcol.nrows();
  mbuf.resize(nrows * sizeof(T));
  T* outdata = static_cast<T*>(mbuf.xptr());

  py::oobj item;
  for (size_t i = i0; i < nrows; ++i) {
    inputcol.get_element(i, &item);
    bool ok = f(item, outdata + i);
    if (!ok) return i;
  }
  for (size_t i = 0; i < i0; ++i) {
    inputcol.get_element(i, &item);
    bool ok = f(item, outdata + i);
    xassert(ok); (void)ok;
  }
  return nrows;
}



//------------------------------------------------------------------------------
// Void
//------------------------------------------------------------------------------

static size_t parse_as_void(const Column& inputcol) {
  size_t i = 0;
  py::oobj item;
  for (; i < inputcol.nrows(); ++i) {
    inputcol.get_element(i, &item);
    if (!item.is_none()) break;
  }
  return i;
}



//------------------------------------------------------------------------------
// Boolean
//------------------------------------------------------------------------------

// Parse list of booleans, i.e. `True`, `False` and `None`.
// static size_t parse_as_bool(const Column& inputcol, Buffer& mbuf, size_t i0)
// {
//   return parse_as_X<int8_t>(inputcol, mbuf, i0,
//             [](const py::oobj& item, int8_t* out) {
//               return item.parse_bool(out) ||
//                      item.parse_none(out) ||
//                      item.parse_numpy_bool(out);
//             });
// }



//------------------------------------------------------------------------------
// Integer
//------------------------------------------------------------------------------

// Parse list of integers, accepting either regular python ints, or
// python bools, or numpy ints, or `None`. The ints are parsed only
// for T==int32_t or T==int64_t. This is because this function is
// used for type autodetection, and we don't want small integers to
// be detected as int8_t or int16_t. Thus, the only way to get an
// auto-detected stype INT8 is to have int8 numpy integers in the
// list, possibly mixed with Nones and booleans.
//
// Integers that are too large for int32/int64 will be promoted to
// stype INT64/FLOAT64 respectively.
//
// template <typename T>
// static size_t parse_as_int(const Column& inputcol, Buffer& mbuf, size_t i0)
// {
//   return parse_as_X<T>(inputcol, mbuf, i0,
//             [](const py::oobj& item, T* out) {
//               return (sizeof(T) >= 4 && item.parse_int(out)) ||
//                      item.parse_none(out) ||
//                      item.parse_numpy_int(out) ||
//                      item.parse_bool(out);
//             });
// }


// static size_t parse_as_int8(const Column& inputcol, Buffer& mbuf, size_t i0)
// {
//   return parse_as_X<int8_t>(inputcol, mbuf, i0,
//             [](const py::oobj& item, int8_t* out) {
//               return item.parse_01(out) ||
//                      item.parse_none(out) ||
//                      item.parse_numpy_int(out) ||
//                      item.parse_bool(out);
//             });
// }

// static size_t parse_as_int16(const Column& inputcol, Buffer& mbuf, size_t i0)
// {
//   return parse_as_X<int16_t>(inputcol, mbuf, i0,
//             [](const py::oobj& item, int16_t* out) {
//               return item.parse_numpy_int(out) ||
//                      item.parse_none(out) ||
//                      item.parse_01(out) ||
//                      item.parse_bool(out);
//             });
// }



//------------------------------------------------------------------------------
// Float
//------------------------------------------------------------------------------

// static size_t parse_as_float32(const Column& inputcol, Buffer& mbuf, size_t i0)
// {
//   return parse_as_X<float>(inputcol, mbuf, i0,
//             [](const py::oobj& item, float* out) {
//               return item.parse_numpy_float(out) ||
//                      item.parse_none(out);
//             });
// }


// static size_t parse_as_float64(const Column& inputcol, Buffer& mbuf, size_t i0)
// {
//   return parse_as_X<double>(inputcol, mbuf, i0,
//             [](const py::oobj& item, double* out) {
//               return item.parse_double(out) ||
//                      item.parse_none(out) ||
//                      item.parse_int(out) ||
//                      item.parse_numpy_float(out) ||
//                      item.parse_bool(out) ||
//                      false;
//             });
// }



//------------------------------------------------------------------------------
// Date32
//------------------------------------------------------------------------------

static size_t parse_as_date32(const Column& inputcol, Buffer& mbuf, size_t i0) {
  return parse_as_X<int32_t>(inputcol, mbuf, i0,
            [](const py::oobj& item, int32_t* out) {
              return item.parse_date_as_date(out) ||
                     item.parse_none(out);
            });
}




//------------------------------------------------------------------------------
// Time64
//------------------------------------------------------------------------------

static size_t parse_as_time64(const Column& inputcol, Buffer& mbuf, size_t i0) {
  return parse_as_X<int64_t>(inputcol, mbuf, i0,
            [](const py::oobj& item, int64_t* out) {
              return item.parse_datetime_as_time(out) ||
                     item.parse_date_as_time(out) ||
                     item.parse_none(out);
            });
}




//------------------------------------------------------------------------------
// String
//------------------------------------------------------------------------------

// template <typename T>
// static size_t parse_as_str(const Column& inputcol, Buffer& offbuf,
//                            Buffer& strbuf)
// {
//   static_assert(std::is_unsigned<T>::value, "Wrong T type");

//   size_t nrows = inputcol.nrows();
//   offbuf.resize((nrows + 1) * sizeof(T));
//   T* offsets = static_cast<T*>(offbuf.wptr()) + 1;
//   offsets[-1] = 0;
//   if (!strbuf) {
//     strbuf.resize(nrows * 4);  // arbitrarily 4 chars per element
//   }
//   char* strptr = static_cast<char*>(strbuf.xptr());

//   T curr_offset = 0;
//   size_t i = 0;
//   py::oobj item;
//   py::oobj tmpitem;
//   for (i = 0; i < nrows; ++i) {
//     inputcol.get_element(i, &item);

//     if (item.is_none()) {
//       offsets[i] = curr_offset ^ dt::GETNA<T>();
//       continue;
//     }
//     if (item.is_string()) {
//       parse_string:
//       dt::CString cstr = item.to_cstring();
//       if (cstr.size()) {
//         T tlen = static_cast<T>(cstr.size());
//         T next_offset = curr_offset + tlen;
//         // Check that length or offset of the string doesn't overflow int32_t
//         if (std::is_same<T, uint32_t>::value &&
//               (static_cast<size_t>(tlen) != cstr.size() ||
//                next_offset < curr_offset)) {
//           break;
//         }
//         // Resize the strbuf if necessary
//         if (strbuf.size() < static_cast<size_t>(next_offset)) {
//           double newsize = static_cast<double>(next_offset) *
//               (static_cast<double>(nrows) / static_cast<double>(i + 1)) * 1.1;
//           strbuf.resize(static_cast<size_t>(newsize));
//           strptr = static_cast<char*>(strbuf.xptr());
//         }
//         std::memcpy(strptr + curr_offset, cstr.data(), cstr.size());
//         curr_offset = next_offset;
//       }
//       offsets[i] = curr_offset;
//       continue;
//     }
//     if (item.is_int() || item.is_float() || item.is_bool()) {
//       tmpitem = item.to_pystring_force();
//       item = tmpitem;
//       goto parse_string;
//     }
//     break;
//   }
//   if (i < nrows) {
//     if (std::is_same<T, uint64_t>::value) {
//       strbuf.resize(0);
//     }
//   } else {
//     strbuf.resize(static_cast<size_t>(curr_offset));
//   }
//   return i;
// }




//------------------------------------------------------------------------------
// Parse controller
//------------------------------------------------------------------------------

static const std::vector<dt::SType>& successors(dt::SType stype) {
  using styvec = std::vector<dt::SType>;
  static styvec s_void = {
    dt::SType::BOOL, dt::SType::INT8, dt::SType::INT16, dt::SType::INT32,
    dt::SType::INT64, dt::SType::FLOAT32, dt::SType::FLOAT64, dt::SType::STR32,
    dt::SType::DATE32, dt::SType::TIME64
  };
  static styvec s_bool8 = {dt::SType::INT8, dt::SType::INT16, dt::SType::INT32, dt::SType::INT64, dt::SType::FLOAT64, dt::SType::STR32};
  static styvec s_int8  = {dt::SType::INT16, dt::SType::INT32, dt::SType::INT64, dt::SType::FLOAT64, dt::SType::STR32};
  static styvec s_int16 = {dt::SType::INT32, dt::SType::INT64, dt::SType::FLOAT64, dt::SType::STR32};
  static styvec s_int32 = {dt::SType::INT64, dt::SType::FLOAT64, dt::SType::STR32};
  static styvec s_int64 = {dt::SType::FLOAT64, dt::SType::STR32};
  static styvec s_float32 = {dt::SType::FLOAT64, dt::SType::STR32};
  static styvec s_float64 = {dt::SType::STR32};
  static styvec s_str32 = {dt::SType::STR64};
  static styvec s_str64 = {};
  static styvec s_date32 = {dt::SType::TIME64};
  static styvec s_time64 = {};

  switch (stype) {
    case dt::SType::VOID:    return s_void;
    case dt::SType::BOOL:    return s_bool8;
    case dt::SType::INT8:    return s_int8;
    case dt::SType::INT16:   return s_int16;
    case dt::SType::INT32:   return s_int32;
    case dt::SType::INT64:   return s_int64;
    case dt::SType::FLOAT32: return s_float32;
    case dt::SType::FLOAT64: return s_float64;
    case dt::SType::STR32:   return s_str32;
    case dt::SType::STR64:   return s_str64;
    case dt::SType::DATE32:  return s_date32;
    case dt::SType::TIME64:  return s_time64;
    default:
      throw RuntimeError() << "Unknown successors of type " << stype;  // LCOV_EXCL_LINE
  }
}


/**
  * Parse `inputcol`, auto-detecting its type.
  */
static Column parse_column_auto_type(const Column& inputcol) {
  xassert(inputcol.type().is_object());
  Buffer databuf, strbuf;
  size_t nrows = inputcol.nrows();

  // We start with the VOID type, which is upwards-compatible with all other
  // types.
  dt::SType stype = dt::SType::VOID;
  size_t i = parse_as_void(inputcol);
  while (i < nrows) {
    size_t j = i;
    for (dt::SType next_stype : successors(stype)) {
      switch (next_stype) {
        // case dt::SType::BOOL:    j = parse_as_bool(inputcol, databuf, i); break;
        // case dt::SType::INT8:    j = parse_as_int8(inputcol, databuf, i); break;
        // case dt::SType::INT16:   j = parse_as_int16(inputcol, databuf, i); break;
        // case dt::SType::INT32:   j = parse_as_int<int32_t>(inputcol, databuf, i); break;
        // case dt::SType::INT64:   j = parse_as_int<int64_t>(inputcol, databuf, i); break;
        // case dt::SType::FLOAT32: j = parse_as_float32(inputcol, databuf, i); break;
        // case dt::SType::FLOAT64: j = parse_as_float64(inputcol, databuf, i); break;
        // case dt::SType::STR32:   j = parse_as_str<uint32_t>(inputcol, databuf, strbuf); break;
        // case dt::SType::STR64:   j = parse_as_str<uint64_t>(inputcol, databuf, strbuf); break;
        case dt::SType::DATE32:  j = parse_as_date32(inputcol, databuf, i); break;
        case dt::SType::TIME64:  j = parse_as_time64(inputcol, databuf, i); break;
        default: continue;  // try another stype
      }
      if (j != i) {
        stype = next_stype;
        break;
      }
    }
    if (j == i) {
      py::oobj item;
      inputcol.get_element(i, &item);
      auto err = TypeError();
      err << "Cannot create column from a python list: element at index "
          << i << " is of type " << item.typeobj();
      if (i) {
        err << ", while previous elements had type `" << stype << "`";
      }
      err << ". If you meant to create a column of type obj64, then you must "
             "request this type explicitly";
      throw err;
    }
    i = j;
  }
  if (stype == dt::SType::STR32 || stype == dt::SType::STR64) {
    return Column::new_string_column(nrows, std::move(databuf), std::move(strbuf));
  }
  else if (stype == dt::SType::VOID) {
    return Column::new_na_column(nrows, dt::SType::VOID);
  }
  else {
    return Column::new_mbuf_column(nrows, stype, std::move(databuf));
  }
}



static Column resolve_column(const Column& inputcol, dt::Type type0)
{
  if (type0) {
    Column out = inputcol.cast(type0);
    out.materialize();
    return out;
  }
  else {
    Column out = new_parse_column_auto_type(inputcol);
    if (!out) out = parse_column_auto_type(inputcol);
    return out;
  }
}




//------------------------------------------------------------------------------
// Column API
//------------------------------------------------------------------------------

Column Column::from_pylist(const py::olist& list, dt::Type type0) {
  Column inputcol(new dt::PyList_ColumnImpl(list));
  return resolve_column(inputcol, type0);
}


Column Column::from_pylist_of_tuples(
    const py::olist& list, size_t index, dt::Type type0)
{
  Column inputcol(new dt::PyTupleList_ColumnImpl(list, index));
  return resolve_column(inputcol, type0);
}


Column Column::from_pylist_of_dicts(
    const py::olist& list, py::robj name, dt::Type type0)
{
  Column inputcol(new dt::PyDictList_ColumnImpl(list, name));
  return resolve_column(inputcol, type0);
}


Column Column::from_range(
    int64_t start, int64_t stop, int64_t step, dt::Type type)
{
  if (type.is_string() || type.is_object() || type.is_boolean()) {
    Column col = Column(new dt::Range_ColumnImpl(start, stop, step, dt::Type()));
    col.cast_inplace(type);
    return col;
  }
  return Column(new dt::Range_ColumnImpl(start, stop, step, type));
}
