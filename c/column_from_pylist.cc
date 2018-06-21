//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#include "column.h"
#include <cstdlib>        // std::abs
#include <limits>         // std::numeric_limits
#include <type_traits>    // std::is_same
#include "py_types.h"     // PyLong_AsInt64AndOverflow
#include "python/float.h" // PyyFloat
#include "python/list.h"  // PyyList
#include "python/long.h"  // PyyLong
#include "utils.h"
#include "utils/exceptions.h"

extern PyObject* Py_One;
extern PyObject* Py_Zero;



//------------------------------------------------------------------------------
// Boolean
//------------------------------------------------------------------------------

/**
 * Convert Python list of objects into a column of boolean type, if possible.
 * The converted values will be written into the provided `membuf` (which will
 * be reallocated to proper size).
 *
 * Return true if conversion was successful, and false if it failed. Upon
 * failure, variable `from` will be set to the index of the variable that was
 * not parsed successfully.
 *
 * This converter recognizes pythonic `True` or number 1 as "true" values,
 * pythonic `False` or number 0 as "false" values, and pythonic `None` as NA.
 * If any other value is encountered, the parse will fail.
 */
static bool parse_as_bool(PyyList& list, MemoryRange& membuf, size_t& from)
{
  size_t nrows = list.size();
  membuf.resize(nrows);
  int8_t* outdata = static_cast<int8_t*>(membuf.wptr());

  // Use the fact that Python stores small integers as singletons, and thus
  // in order to check whether a PyObject* is integer 0 or 1 it's enough to
  // check whether the objects are the same.
  for (size_t i = 0; i < nrows; ++i) {
    PyObj item = list[i];

    if (item.is_none()) outdata[i] = GETNA<int8_t>();
    else if (item.is_true()) outdata[i] = 1;
    else if (item.is_false()) outdata[i] = 0;
    else {
      from = i;
      return false;
    }
  }
  return true;
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
static void force_as_bool(PyyList& list, MemoryRange& membuf)
{
  size_t nrows = list.size();
  membuf.resize(nrows);
  int8_t* outdata = static_cast<int8_t*>(membuf.wptr());

  for (size_t i = 0; i < nrows; ++i) {
    PyObj item = list[i];
    outdata[i] = item.__bool__();
  }
}



//------------------------------------------------------------------------------
// Integer
//------------------------------------------------------------------------------

/**
 * Convert Python list of objects into a column of integer<T> type, if possible.
 * The converted values will be written into the provided `membuf` (which will
 * be reallocated to proper size). Returns true if conversion was successful,
 * otherwise returns false and sets `from` to the index within the source list
 * of the element that could not be parsed.
 *
 * This converter recognizes either python `None`, or pythonic `int` object.
 * Any other pythonic object will cause the parser to fail. Likewise, the
 * parser will fail if the `int` value does not fit into the range of type `T`.
 */
template <typename T>
static bool parse_as_int(PyyList& list, MemoryRange& membuf, size_t& from)
{
  size_t nrows = list.size();
  membuf.resize(nrows * sizeof(T));
  T* outdata = static_cast<T*>(membuf.wptr());

  int overflow = 0;
  for (int j = 0; j < 2; ++j) {
    size_t ifrom = j ? 0 : from;
    size_t ito   = j ? from : nrows;

    for (size_t i = ifrom; i < ito; ++i) {
      PyObj item = list[i];

      if (item.is_none()) {
        outdata[i] = GETNA<T>();
        continue;
      }
      if (item.is_long()) {
        PyyLong litem = item;
        outdata[i] = litem.value<T>(&overflow);
        if (!overflow) continue;
      }
      from = i;
      return false;
    }
  }
  return true;
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
static void force_as_int(PyyList& list, MemoryRange& membuf)
{
  size_t nrows = list.size();
  membuf.resize(nrows * sizeof(T));
  T* outdata = static_cast<T*>(membuf.wptr());

  for (size_t i = 0; i < nrows; ++i) {
    PyObj item = list[i];
    if (item.is_none()) {
      outdata[i] = GETNA<T>();
      continue;
    }
    PyyLong litem = item.is_long()? (PyyLong)item : item.__int__();
    outdata[i] = litem.masked_value<T>();
  }
}



//------------------------------------------------------------------------------
// Float
//------------------------------------------------------------------------------

/**
 * We don't attempt to parse as float because Python internally stores numbers
 * as doubles, and it's extremely hard to determine whether that number should
 * have been a float instead...
 */
static bool parse_as_double(PyyList& list, MemoryRange& membuf, size_t& from)
{
  size_t nrows = list.size();
  membuf.resize(nrows * sizeof(double));
  double* outdata = static_cast<double*>(membuf.wptr());

  int overflow = 0;
  for (int j = 0; j < 2; ++j) {
    size_t ifrom = j ? 0 : from;
    size_t ito   = j ? from : nrows;
    for (size_t i = ifrom; i < ito; ++i) {
      PyObj item = list[i];

      if (item.is_none()) {
        outdata[i] = GETNA<double>();
        continue;
      }
      if (item.is_long()) {
        PyyLong litem = item;
        outdata[i] = litem.value<double>(&overflow);
        continue;
      }
      if (item.is_float()) {
        outdata[i] = item.as_double();
        continue;
      }
      from = i;
      return false;
    }
  }
  PyErr_Clear();  // in case an overflow occurred
  return true;
}


template <typename T>
static void force_as_real(PyyList& list, MemoryRange& membuf)
{
  size_t nrows = list.size();
  membuf.resize(nrows * sizeof(T));
  T* outdata = static_cast<T*>(membuf.wptr());

  int overflow = 0;
  for (size_t i = 0; i < nrows; ++i) {
    PyObj item = list[i];

    if (item.is_none()) {
      outdata[i] = GETNA<T>();
      continue;
    }
    if (item.is_long()) {
      PyyLong litem = item;
      outdata[i] = litem.value<T>(&overflow);
      continue;
    }
    PyyFloat fitem = item.is_float()? item : item.__float__();
    outdata[i] = fitem.value<T>();
  }
  PyErr_Clear();  // in case an overflow occurred
}



//------------------------------------------------------------------------------
// String
//------------------------------------------------------------------------------

template <typename T>
static bool parse_as_str(PyyList& list, MemoryRange& offbuf,
                         MemoryRange& strbuf)
{
  size_t nrows = list.size();
  offbuf.resize((nrows + 1) * sizeof(T));
  T* offsets = static_cast<T*>(offbuf.wptr()) + 1;
  offsets[-1] = -1;
  if (!strbuf) {
    strbuf.resize(nrows * 4);  // arbitrarily 4 chars per element
  }

  T curr_offset = 1;
  size_t i = 0;
  for (i = 0; i < nrows; ++i) {
    PyObj item = list[i];

    if (item.is_none()) {
      offsets[i] = -curr_offset;
      continue;
    }
    if (item.is_string()) {
      size_t len = 0;
      const char* cstr = item.as_cstring(&len);
      if (len) {
        T tlen = static_cast<T>(len);
        T next_offset = curr_offset + tlen;
        // Check that length or offset of the string doesn't overflow int32_t
        if (std::is_same<T, int32_t>::value &&
            (static_cast<size_t>(tlen) != len || next_offset < curr_offset)) {
          break;
        }
        // Resize the strbuf if necessary
        if (strbuf.size() < static_cast<size_t>(next_offset)) {
          double newsize = static_cast<double>(next_offset) *
                           (static_cast<double>(nrows) / (i + 1)) * 1.1;
          strbuf.resize(static_cast<size_t>(newsize));
        }
        std::memcpy(strbuf.wptr(static_cast<size_t>(curr_offset) - 1), cstr, len);
        curr_offset = next_offset;
      }
      offsets[i] = curr_offset;
      continue;
    }
    break;
  }
  if (i < nrows) {
    if (std::is_same<T, int64_t>::value) {
      strbuf.resize(0);
    }
    return false;
  } else {
    strbuf.resize(static_cast<size_t>(curr_offset - 1));
    return true;
  }
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
static void force_as_str(PyyList& list, MemoryRange& offbuf,
                         MemoryRange& strbuf)
{
  size_t nrows = list.size();
  if (nrows > std::numeric_limits<T>::max()) {
    throw ValueError()
      << "Cannot store " << nrows << " elements in a str32 column";
  }
  offbuf.resize((nrows + 1) * sizeof(T));
  T* offsets = static_cast<T*>(offbuf.wptr()) + 1;
  offsets[-1] = -1;
  if (!strbuf) {
    strbuf.resize(nrows * 4);
  }

  T curr_offset = 1;
  for (size_t i = 0; i < nrows; ++i) {
    PyObj item = list[i];

    if (item.is_none()) {
      offsets[i] = -curr_offset;
      continue;
    }
    if (!item.is_string()) {
      item = item.__str__();
    }
    if (item.is_string()) {
      size_t len = 0;
      const char* cstr = item.as_cstring(&len);
      if (len) {
        T tlen = static_cast<T>(len);
        T next_offset = curr_offset + tlen;
        if (std::is_same<T, int32_t>::value &&
            (static_cast<size_t>(tlen) != len || next_offset < curr_offset)) {
          offsets[i] = -curr_offset;
          continue;
        }
        if (strbuf.size() < static_cast<size_t>(next_offset)) {
          double newsize = static_cast<double>(next_offset) *
                           (static_cast<double>(nrows) / (i + 1)) * 1.1;
          strbuf.resize(static_cast<size_t>(newsize));
        }
        std::memcpy(strbuf.wptr(static_cast<size_t>(curr_offset) - 1), cstr, len);
        curr_offset = next_offset;
      }
      offsets[i] = curr_offset;
      continue;
    } else {
      offsets[i] = -curr_offset;
    }
  }
  strbuf.resize(static_cast<size_t>(curr_offset - 1));
}



//------------------------------------------------------------------------------
// Object
//------------------------------------------------------------------------------

static bool parse_as_pyobj(PyyList& list, MemoryRange& membuf)
{
  size_t nrows = list.size();
  membuf.resize(nrows * sizeof(PyObject*));
  PyObject** outdata = static_cast<PyObject**>(membuf.wptr());

  for (size_t i = 0; i < nrows; ++i) {
    PyObj item = list[i];
    if (item.is_float() && std::isnan(item.as_double())) {
      item = PyObj::none();
    }
    outdata[i] = item.as_pyobject();
  }
  return true;
}


// No "force" method, because `parse_as_pyobj()` is already capable of
// processing any pylist.



//------------------------------------------------------------------------------
// Parse controller
//------------------------------------------------------------------------------

static int find_next_stype(int curr_stype, int stype0, int ltype0) {
  if (stype0 > 0) {
    return stype0;
  }
  if (stype0 < 0) {
    return std::min(curr_stype + 1, -stype0);
  }
  if (ltype0 > 0) {
    for (int i = curr_stype + 1; i < DT_STYPES_COUNT; i++) {
      if (i >= ST_STRING_FCHAR && i <= ST_DATETIME_I2_MONTH) continue;
      if (stype_info[i].ltype == ltype0) return i;
    }
    return curr_stype;
  }
  if (ltype0 < 0) {
    for (int i = curr_stype + 1; i < DT_STYPES_COUNT; i++) {
      if (i >= ST_STRING_FCHAR && i <= ST_DATETIME_I2_MONTH) continue;
      if (stype_info[i].ltype <= -ltype0) return i;
    }
    return curr_stype;
  }
  if (curr_stype == DT_STYPES_COUNT - 1) {
    return curr_stype;
  }
  return (curr_stype + 1) % DT_STYPES_COUNT;
}



Column* Column::from_pylist(PyyList& list, int stype0, int ltype0)
{
  if (stype0 && ltype0) {
    throw ValueError() << "Cannot fix both stype and ltype";
  }

  MemoryRange membuf;
  MemoryRange strbuf;
  int stype = find_next_stype(0, stype0, ltype0);
  size_t i = 0;
  while (stype) {
    int next_stype = find_next_stype(stype, stype0, ltype0);
    if (stype == next_stype) {
      switch (stype) {
        case ST_BOOLEAN_I1:      force_as_bool(list, membuf); break;
        case ST_INTEGER_I1:      force_as_int<int8_t>(list, membuf); break;
        case ST_INTEGER_I2:      force_as_int<int16_t>(list, membuf); break;
        case ST_INTEGER_I4:      force_as_int<int32_t>(list, membuf); break;
        case ST_INTEGER_I8:      force_as_int<int64_t>(list, membuf); break;
        case ST_REAL_F4:         force_as_real<float>(list, membuf); break;
        case ST_REAL_F8:         force_as_real<double>(list, membuf); break;
        case ST_STRING_I4_VCHAR: force_as_str<int32_t>(list, membuf, strbuf); break;
        case ST_STRING_I8_VCHAR: force_as_str<int64_t>(list, membuf, strbuf); break;
        case ST_OBJECT_PYPTR:    parse_as_pyobj(list, membuf); break;
        default:
          throw RuntimeError()
            << "Unable to create Column of type " << stype << " from list";
      }
      break; // while(stype)
    } else {
      bool ret = false;
      switch (stype) {
        case ST_BOOLEAN_I1:      ret = parse_as_bool(list, membuf, i); break;
        case ST_INTEGER_I1:      ret = parse_as_int<int8_t>(list, membuf, i); break;
        case ST_INTEGER_I2:      ret = parse_as_int<int16_t>(list, membuf, i); break;
        case ST_INTEGER_I4:      ret = parse_as_int<int32_t>(list, membuf, i); break;
        case ST_INTEGER_I8:      ret = parse_as_int<int64_t>(list, membuf, i); break;
        case ST_REAL_F8:         ret = parse_as_double(list, membuf, i); break;
        case ST_STRING_I4_VCHAR: ret = parse_as_str<int32_t>(list, membuf, strbuf); break;
        case ST_STRING_I8_VCHAR: ret = parse_as_str<int64_t>(list, membuf, strbuf); break;
        case ST_OBJECT_PYPTR:    ret = parse_as_pyobj(list, membuf); break;
        default: /* do nothing -- not all STypes are currently implemented. */ break;
      }
      if (ret) break;
      stype = next_stype;
    }
  }
  Column* col = Column::new_column(static_cast<SType>(stype));
  if (stype == ST_OBJECT_PYPTR) {
    membuf.set_pyobjects(/* clear_data = */ false);
  }
  if (stype == ST_STRING_I4_VCHAR || stype == ST_STRING_I8_VCHAR) {
    col->replace_buffer(std::move(membuf), std::move(strbuf));
  } else {
    col->replace_buffer(std::move(membuf));
  }
  return col;
}
