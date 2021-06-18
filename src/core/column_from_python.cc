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
#include "utils/misc.h"
#include "column.h"
#include "stype.h"


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
static size_t parse_as_bool(const Column& inputcol, Buffer& mbuf, size_t i0)
{
  return parse_as_X<int8_t>(inputcol, mbuf, i0,
            [](const py::oobj& item, int8_t* out) {
              return item.parse_bool(out) ||
                     item.parse_none(out) ||
                     item.parse_numpy_bool(out);
            });
}


/**
 * Similar to `parse_as_bool()`, this function parses the provided Python list
 * and converts it into a boolean column, which is written into `membuf`.
 *
 * Unlike the previous, this function never fails and forces all the values
 * into proper booleans. In particular, Python `None` will be treated as NA
 * while all other items will be pythonically cast into booleans, which is
 * equivalent to using `bool(x)` or `not(not x)` in Python. If such conversion
 * fails for any reason (for example, method `__bool__()` raised an exception)
 * then the value will be converted into NA.
 */
static Column force_as_bool(const Column& inputcol)
{
  size_t nrows = inputcol.nrows();
  Buffer data = Buffer::mem(nrows);
  auto outdata = static_cast<int8_t*>(data.xptr());

  py::oobj item;
  for (size_t i = 0; i < nrows; ++i) {
    inputcol.get_element(i, &item);
    outdata[i] = item.to_bool_force();
  }
  return Column::new_mbuf_column(nrows, dt::SType::BOOL, std::move(data));
}



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
template <typename T>
static size_t parse_as_int(const Column& inputcol, Buffer& mbuf, size_t i0)
{
  return parse_as_X<T>(inputcol, mbuf, i0,
            [](const py::oobj& item, T* out) {
              return (sizeof(T) >= 4 && item.parse_int(out)) ||
                     item.parse_none(out) ||
                     item.parse_numpy_int(out) ||
                     item.parse_bool(out);
            });
}


static size_t parse_as_int8(const Column& inputcol, Buffer& mbuf, size_t i0)
{
  return parse_as_X<int8_t>(inputcol, mbuf, i0,
            [](const py::oobj& item, int8_t* out) {
              return item.parse_01(out) ||
                     item.parse_none(out) ||
                     item.parse_numpy_int(out) ||
                     item.parse_bool(out);
            });
}

static size_t parse_as_int16(const Column& inputcol, Buffer& mbuf, size_t i0)
{
  return parse_as_X<int16_t>(inputcol, mbuf, i0,
            [](const py::oobj& item, int16_t* out) {
              return item.parse_numpy_int(out) ||
                     item.parse_none(out) ||
                     item.parse_01(out) ||
                     item.parse_bool(out);
            });
}


/**
 * Force-convert python list into an integer column of type <T> (the data will
 * be written into the provided buffer).
 *
 * Each element will be converted into an integer using python `int(x)` call.
 * If the call fails, that element will become an NA. If an integer value is
 * outside of the range of `T`, it will be reduced modulo `MAX<T> + 1` (same
 * as C++'s `static_cast<T>`).
 */
template <typename T>
static Column force_as_int(const Column& inputcol) {
  size_t nrows = inputcol.nrows();
  Buffer databuf = Buffer::mem(nrows * sizeof(T));
  auto outdata = static_cast<T*>(databuf.wptr());

  py::oobj item;
  for (size_t i = 0; i < nrows; ++i) {
    inputcol.get_element(i, &item);
    if (item.is_none()) {
      outdata[i] = dt::GETNA<T>();
      continue;
    }
    py::oint litem = item.to_pyint_force();
    outdata[i] = litem.mvalue<T>();
  }
  return Column::new_mbuf_column(nrows, dt::stype_from<T>, std::move(databuf));
}



//------------------------------------------------------------------------------
// Float
//------------------------------------------------------------------------------

static size_t parse_as_float32(const Column& inputcol, Buffer& mbuf, size_t i0)
{
  return parse_as_X<float>(inputcol, mbuf, i0,
            [](const py::oobj& item, float* out) {
              return item.parse_numpy_float(out) ||
                     item.parse_none(out);
            });
}


static size_t parse_as_float64(const Column& inputcol, Buffer& mbuf, size_t i0)
{
  return parse_as_X<double>(inputcol, mbuf, i0,
            [](const py::oobj& item, double* out) {
              return item.parse_double(out) ||
                     item.parse_none(out) ||
                     item.parse_int(out) ||
                     item.parse_numpy_float(out) ||
                     item.parse_bool(out) ||
                     false;
            });
}


template <typename T>
static Column force_as_real(const Column& inputcol)
{
  size_t nrows = inputcol.nrows();
  Buffer databuf = Buffer::mem(nrows * sizeof(T));
  auto outdata = static_cast<T*>(databuf.xptr());

  int overflow = 0;
  py::oobj item;
  for (size_t i = 0; i < nrows; ++i) {
    inputcol.get_element(i, &item);

    if (item.is_none()) {
      outdata[i] = dt::GETNA<T>();
      continue;
    }
    if (item.is_int()) {
      py::oint litem = item.to_pyint();
      outdata[i] = litem.ovalue<T>(&overflow);
      continue;
    }
    py::ofloat fitem = item.to_pyfloat_force();
    outdata[i] = fitem.value<T>();
  }
  PyErr_Clear();  // in case an overflow occurred
  return Column::new_mbuf_column(nrows, dt::stype_from<T>, std::move(databuf));
}



//------------------------------------------------------------------------------
// Date32
//------------------------------------------------------------------------------

static size_t parse_as_date32(const Column& inputcol, Buffer& mbuf, size_t i0) {
  return parse_as_X<int32_t>(inputcol, mbuf, i0,
            [](const py::oobj& item, int32_t* out) {
              return item.parse_date(out) ||
                     item.parse_none(out);
            });
}


static Column force_as_date32(const Column& inputcol) {
  size_t nrows = inputcol.nrows();
  Buffer databuf = Buffer::mem(nrows * sizeof(int32_t));
  auto outdata = static_cast<int32_t*>(databuf.xptr());

  int overflow = 0;
  py::oobj item;
  for (size_t i = 0; i < nrows; ++i) {
    inputcol.get_element(i, &item);
    if (item.is_date()) {
      outdata[i] = item.to_odate().get_days();
    }
    else if (item.is_int()) {
      outdata[i] = item.to_pyint().ovalue<int32_t>(&overflow);
    }
    else if (item.is_float()) {
      outdata[i] = static_cast<int32_t>(item.to_double());
    }
    else {
      outdata[i] = dt::GETNA<int32_t>();
    }
  }
  PyErr_Clear();  // in case an overflow occurred
  return Column::new_mbuf_column(nrows, dt::SType::DATE32, std::move(databuf));
}




//------------------------------------------------------------------------------
// Time64
//------------------------------------------------------------------------------

static size_t parse_as_time64(const Column& inputcol, Buffer& mbuf, size_t i0) {
  return parse_as_X<int64_t>(inputcol, mbuf, i0,
            [](const py::oobj& item, int64_t* out) {
              return item.parse_datetime(out) ||
                     item.parse_date(out) ||
                     item.parse_none(out);
            });
}




//------------------------------------------------------------------------------
// String
//------------------------------------------------------------------------------

template <typename T>
static size_t parse_as_str(const Column& inputcol, Buffer& offbuf,
                           Buffer& strbuf)
{
  static_assert(std::is_unsigned<T>::value, "Wrong T type");

  size_t nrows = inputcol.nrows();
  offbuf.resize((nrows + 1) * sizeof(T));
  T* offsets = static_cast<T*>(offbuf.wptr()) + 1;
  offsets[-1] = 0;
  if (!strbuf) {
    strbuf.resize(nrows * 4);  // arbitrarily 4 chars per element
  }
  char* strptr = static_cast<char*>(strbuf.xptr());

  T curr_offset = 0;
  size_t i = 0;
  py::oobj item;
  py::oobj tmpitem;
  for (i = 0; i < nrows; ++i) {
    inputcol.get_element(i, &item);

    if (item.is_none()) {
      offsets[i] = curr_offset ^ dt::GETNA<T>();
      continue;
    }
    if (item.is_string()) {
      parse_string:
      dt::CString cstr = item.to_cstring();
      if (cstr.size()) {
        T tlen = static_cast<T>(cstr.size());
        T next_offset = curr_offset + tlen;
        // Check that length or offset of the string doesn't overflow int32_t
        if (std::is_same<T, uint32_t>::value &&
              (static_cast<size_t>(tlen) != cstr.size() ||
               next_offset < curr_offset)) {
          break;
        }
        // Resize the strbuf if necessary
        if (strbuf.size() < static_cast<size_t>(next_offset)) {
          double newsize = static_cast<double>(next_offset) *
              (static_cast<double>(nrows) / static_cast<double>(i + 1)) * 1.1;
          strbuf.resize(static_cast<size_t>(newsize));
          strptr = static_cast<char*>(strbuf.xptr());
        }
        std::memcpy(strptr + curr_offset, cstr.data(), cstr.size());
        curr_offset = next_offset;
      }
      offsets[i] = curr_offset;
      continue;
    }
    if (item.is_int() || item.is_float() || item.is_bool()) {
      tmpitem = item.to_pystring_force();
      item = tmpitem;
      goto parse_string;
    }
    break;
  }
  if (i < nrows) {
    if (std::is_same<T, uint64_t>::value) {
      strbuf.resize(0);
    }
  } else {
    strbuf.resize(static_cast<size_t>(curr_offset));
  }
  return i;
}


/**
 * Parse the provided `list` of Python objects into a String column (or,
 * more precisely, into 2 memory buffers `offbuf` and `strbuf`). The `strbuf`
 * buffer may be a null pointer, in which case it will be created.
 *
 * This function coerces all values into strings, regardless of their type.
 * If for any reason such coercion is not possible (for example, it raises an
 * exception, or the result doesn't fit into str32, etc) then the corresponding
 * value will be replaced with NA. The only time this function raises an
 * exception is when the source list has more then MAX_INT32 elements and `T` is
 * `int32_t`.
 */
template <typename T>
static Column force_as_str(const Column& inputcol)
{
  size_t nrows = inputcol.nrows();
  if (nrows > std::numeric_limits<T>::max()) {
    throw ValueError()
      << "Cannot store " << nrows << " elements in a str32 column";
  }
  Buffer strbuf = Buffer::mem(nrows * 4);
  Buffer offbuf = Buffer::mem((nrows + 1) * sizeof(T));
  char* strptr = static_cast<char*>(strbuf.xptr());
  auto offsets = static_cast<T*>(offbuf.xptr()) + 1;
  offsets[-1] = 0;

  T curr_offset = 0;
  py::oobj item;
  py::oobj tmpitem;
  for (size_t i = 0; i < nrows; ++i) {
    inputcol.get_element(i, &item);

    if (item.is_none()) {
      offsets[i] = curr_offset ^ dt::GETNA<T>();
      continue;
    }
    if (!item.is_string()) {
      tmpitem = item.to_pystring_force();
      item = tmpitem;
    }
    if (item.is_string()) {
      dt::CString cstr = item.to_cstring();
      if (cstr.size()) {
        T tlen = static_cast<T>(cstr.size());
        T next_offset = curr_offset + tlen;
        if (std::is_same<T, int32_t>::value &&
              (static_cast<size_t>(tlen) != cstr.size() ||
               next_offset < curr_offset)) {
          offsets[i] = curr_offset ^ dt::GETNA<T>();
          continue;
        }
        if (strbuf.size() < static_cast<size_t>(next_offset)) {
          double newsize = static_cast<double>(next_offset) *
              (static_cast<double>(nrows) / static_cast<double>(i + 1)) * 1.1;
          strbuf.resize(static_cast<size_t>(newsize));
          strptr = static_cast<char*>(strbuf.xptr());
        }
        std::memcpy(strptr + curr_offset, cstr.data(), cstr.size());
        curr_offset = next_offset;
      }
      offsets[i] = curr_offset;
      continue;
    } else {
      offsets[i] = curr_offset ^ dt::GETNA<T>();
    }
  }
  strbuf.resize(curr_offset);
  return Column::new_string_column(nrows, std::move(offbuf), std::move(strbuf));
}



//------------------------------------------------------------------------------
// Object
//------------------------------------------------------------------------------

static Column force_as_pyobj(const Column& inputcol)
{
  size_t nrows = inputcol.nrows();
  Buffer databuf = Buffer::mem(nrows * sizeof(PyObject*));
  auto out = static_cast<PyObject**>(databuf.xptr());

  py::oobj item;
  for (size_t i = 0; i < nrows; ++i) {
    inputcol.get_element(i, &item);
    if (item.is_float() && std::isnan(item.to_double())) {
      out[i] = py::None().release();
    } else {
      out[i] = std::move(item).release();
    }
  }
  databuf.set_pyobjects(/* clear_data = */ false);
  return Column::new_mbuf_column(nrows, dt::SType::OBJ, std::move(databuf));
}




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
  * Parse `inputcol`, auto-detecting its stype.
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
        case dt::SType::BOOL:    j = parse_as_bool(inputcol, databuf, i); break;
        case dt::SType::INT8:    j = parse_as_int8(inputcol, databuf, i); break;
        case dt::SType::INT16:   j = parse_as_int16(inputcol, databuf, i); break;
        case dt::SType::INT32:   j = parse_as_int<int32_t>(inputcol, databuf, i); break;
        case dt::SType::INT64:   j = parse_as_int<int64_t>(inputcol, databuf, i); break;
        case dt::SType::FLOAT32: j = parse_as_float32(inputcol, databuf, i); break;
        case dt::SType::FLOAT64: j = parse_as_float64(inputcol, databuf, i); break;
        case dt::SType::STR32:   j = parse_as_str<uint32_t>(inputcol, databuf, strbuf); break;
        case dt::SType::STR64:   j = parse_as_str<uint64_t>(inputcol, databuf, strbuf); break;
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
        err << ", while previous elements had type `" << stype;
      }
      err << "`. If you meant to create a column of type obj64, then you must "
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


/**
  * Parse `inputcol` forcing it into the specific stype.
  */
static Column parse_column_fixed_type(const Column& inputcol, dt::Type type) {
  switch (type.stype()) {
    case dt::SType::VOID:    return Column::new_na_column(inputcol.nrows(), dt::SType::VOID);
    case dt::SType::BOOL:    return force_as_bool(inputcol);
    case dt::SType::INT8:    return force_as_int<int8_t>(inputcol);
    case dt::SType::INT16:   return force_as_int<int16_t>(inputcol);
    case dt::SType::INT32:   return force_as_int<int32_t>(inputcol);
    case dt::SType::INT64:   return force_as_int<int64_t>(inputcol);
    case dt::SType::FLOAT32: return force_as_real<float>(inputcol);
    case dt::SType::FLOAT64: return force_as_real<double>(inputcol);
    case dt::SType::STR32:   return force_as_str<uint32_t>(inputcol);
    case dt::SType::STR64:   return force_as_str<uint64_t>(inputcol);
    case dt::SType::DATE32:  return force_as_date32(inputcol);
    case dt::SType::OBJ:     return force_as_pyobj(inputcol);
    default:
      throw ValueError() << "Unable to create Column of type `"
                         << type << "` from list";
  }
}


static Column resolve_column(const Column& inputcol, dt::Type type0)
{
  if (type0) {
    return parse_column_fixed_type(inputcol, type0);
  }
  else {
    return parse_column_auto_type(inputcol);
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
