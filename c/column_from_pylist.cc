//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018-2019
//------------------------------------------------------------------------------
#include <cstdlib>         // std::abs
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
                         bool(*f)(py::robj, T*))
{
  size_t nrows = inputcol.nrows();
  mbuf.resize(nrows * sizeof(T));
  T* outdata = static_cast<T*>(mbuf.xptr());

  py::robj item;
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
// Boolean
//------------------------------------------------------------------------------

// Parse list of booleans, i.e. `True`, `False` and `None`.
static size_t parse_as_bool(const Column& inputcol, Buffer& mbuf, size_t i0)
{
  return parse_as_X<int8_t>(inputcol, mbuf, i0,
            [](py::robj item, int8_t* out) {
              return item.parse_bool(out) ||
                     item.parse_none(out);
            });
}


// Parse list of "weak" booleans, i.e. `True`, `False`, `1`, `0`
// and `None`. These will be converted to SType::INT8 column.
//
static size_t parse_as_int01(const Column& inputcol, Buffer& mbuf, size_t i0)
{
  return parse_as_X<int8_t>(inputcol, mbuf, i0,
            [](py::robj item, int8_t* out) {
              return item.parse_bool(out) ||
                     item.parse_none(out) ||
                     item.parse_01(out);
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
static void force_as_bool(const Column& inputcol, Buffer& mbuf)
{
  size_t nrows = inputcol.nrows();
  mbuf.resize(nrows);
  auto outdata = static_cast<int8_t*>(mbuf.xptr());

  py::robj item;
  for (size_t i = 0; i < nrows; ++i) {
    inputcol.get_element(i, &item);
    outdata[i] = item.to_bool_force();
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
static size_t parse_as_int(const Column& inputcol, Buffer& mbuf, size_t i0)
{
  return parse_as_X<T>(inputcol, mbuf, i0,
            [](py::robj item, T* out) {
              return (sizeof(T) >= 4 && item.parse_int_no_overflow(out)) ||
                     item.parse_none(out) ||
                     item.parse_numpy_int(out) ||
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
static void force_as_int(const Column& inputcol, Buffer& membuf)
{
  size_t nrows = inputcol.nrows();
  membuf.resize(nrows * sizeof(T));
  T* outdata = static_cast<T*>(membuf.wptr());

  py::robj item;
  for (size_t i = 0; i < nrows; ++i) {
    inputcol.get_element(i, &item);
    if (item.is_none()) {
      outdata[i] = GETNA<T>();
      continue;
    }
    py::oint litem = item.to_pyint_force();
    outdata[i] = litem.mvalue<T>();
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
template <typename T>
static bool parse_as_real(const Column& inputcol, Buffer& membuf, size_t& from)
{
  size_t nrows = inputcol.nrows();
  membuf.resize(nrows * sizeof(T));
  T* outdata = static_cast<T*>(membuf.wptr());

  int overflow = 0;
  py::robj item;
  for (int j = 0; j < 2; ++j) {
    size_t ifrom = j ? 0 : from;
    size_t ito   = j ? from : nrows;
    for (size_t i = ifrom; i < ito; ++i) {
      inputcol.get_element(i, &item);

      if (item.is_none()) {
        outdata[i] = GETNA<T>();
        continue;
      }
      if (std::is_same<T, double>::value) {
        if (item.is_int()) {
          py::oint litem = item.to_pyint();
          outdata[i] = static_cast<T>(litem.ovalue<double>(&overflow));
          continue;
        }
        if (item.is_float()) {
          outdata[i] = static_cast<T>(item.to_double());
          continue;
        }
      }
      int r = item.is_numpy_float();
      if (r && r <= int(sizeof(T))) {
        outdata[i] = static_cast<T>(item.to_double());
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
static void force_as_real(const Column& inputcol, Buffer& membuf)
{
  size_t nrows = inputcol.nrows();
  membuf.resize(nrows * sizeof(T));
  T* outdata = static_cast<T*>(membuf.wptr());

  int overflow = 0;
  py::robj item;
  for (size_t i = 0; i < nrows; ++i) {
    inputcol.get_element(i, &item);

    if (item.is_none()) {
      outdata[i] = GETNA<T>();
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
}



//------------------------------------------------------------------------------
// String
//------------------------------------------------------------------------------

template <typename T>
static bool parse_as_str(const Column& inputcol, Buffer& offbuf,
                         Buffer& strbuf)
{
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
  py::robj item;
  for (i = 0; i < nrows; ++i) {
    inputcol.get_element(i, &item);

    if (item.is_none()) {
      offsets[i] = curr_offset ^ GETNA<T>();
      continue;
    }
    if (item.is_string()) {
      CString cstr = item.to_cstring();
      if (cstr.size) {
        T tlen = static_cast<T>(cstr.size);
        T next_offset = curr_offset + tlen;
        // Check that length or offset of the string doesn't overflow int32_t
        if (std::is_same<T, int32_t>::value &&
              (static_cast<int64_t>(tlen) != cstr.size ||
               next_offset < curr_offset)) {
          break;
        }
        // Resize the strbuf if necessary
        if (strbuf.size() < static_cast<size_t>(next_offset)) {
          double newsize = static_cast<double>(next_offset) *
                           (static_cast<double>(nrows) / (i + 1)) * 1.1;
          strbuf.resize(static_cast<size_t>(newsize));
          strptr = static_cast<char*>(strbuf.xptr());
        }
        std::memcpy(strptr + curr_offset, cstr.ch,
                    static_cast<size_t>(cstr.size));
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
    strbuf.resize(static_cast<size_t>(curr_offset));
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
static void force_as_str(const Column& inputcol, Buffer& offbuf,
                         Buffer& strbuf)
{
  size_t nrows = inputcol.nrows();
  if (nrows > std::numeric_limits<T>::max()) {
    throw ValueError()
      << "Cannot store " << nrows << " elements in a str32 column";
  }
  offbuf.resize((nrows + 1) * sizeof(T));
  T* offsets = static_cast<T*>(offbuf.wptr()) + 1;
  offsets[-1] = 0;
  if (!strbuf) {
    strbuf.resize(nrows * 4);
  }
  char* strptr = static_cast<char*>(strbuf.xptr());

  T curr_offset = 0;
  py::robj item;
  for (size_t i = 0; i < nrows; ++i) {
    inputcol.get_element(i, &item);

    if (item.is_none()) {
      offsets[i] = curr_offset ^ GETNA<T>();
      continue;
    }
    if (!item.is_string()) {
      item = item.to_pystring_force();
    }
    if (item.is_string()) {
      CString cstr = item.to_cstring();
      if (cstr.size) {
        T tlen = static_cast<T>(cstr.size);
        T next_offset = curr_offset + tlen;
        if (std::is_same<T, int32_t>::value &&
              (static_cast<int64_t>(tlen) != cstr.size ||
               next_offset < curr_offset)) {
          offsets[i] = curr_offset ^ GETNA<T>();
          continue;
        }
        if (strbuf.size() < static_cast<size_t>(next_offset)) {
          double newsize = static_cast<double>(next_offset) *
                           (static_cast<double>(nrows) / (i + 1)) * 1.1;
          strbuf.resize(static_cast<size_t>(newsize));
          strptr = static_cast<char*>(strbuf.xptr());
        }
        std::memcpy(strptr + curr_offset, cstr.ch,
                    static_cast<size_t>(cstr.size));
        curr_offset = next_offset;
      }
      offsets[i] = curr_offset;
      continue;
    } else {
      offsets[i] = curr_offset ^ GETNA<T>();
    }
  }
  strbuf.resize(curr_offset);
}



//------------------------------------------------------------------------------
// Object
//------------------------------------------------------------------------------

static bool parse_as_pyobj(const Column& inputcol, Buffer& membuf)
{
  size_t nrows = inputcol.nrows();
  membuf.resize(nrows * sizeof(PyObject*));
  PyObject** outdata = static_cast<PyObject**>(membuf.wptr());

  py::robj item;
  for (size_t i = 0; i < nrows; ++i) {
    inputcol.get_element(i, &item);
    if (item.is_float() && std::isnan(item.to_double())) {
      outdata[i] = py::None().release();
    } else {
      outdata[i] = py::oobj(item).release();
    }
  }
  return true;
}


// No "force" method, because `parse_as_pyobj()` is already capable of
// processing any pylist.



//------------------------------------------------------------------------------
// Parse controller
//------------------------------------------------------------------------------

static SType find_next_stype(SType curr_stype, int stype0) {
  int istype = static_cast<int>(curr_stype);
  if (stype0 > 0) {
    return static_cast<SType>(stype0);
  }
  if (stype0 < 0) {
    return static_cast<SType>(std::min(istype + 1, -stype0));
  }
  if (istype == DT_STYPES_COUNT - 1) {
    return curr_stype;
  }
  return static_cast<SType>((istype + 1) % int(DT_STYPES_COUNT));
}



static Column resolve_column(const Column& inputcol, int stype0)
{
  Buffer membuf;
  Buffer strbuf;
  SType stype = find_next_stype(SType::VOID, stype0);
  size_t nrows = inputcol.nrows();
  size_t i = 0;
  while (stype != SType::VOID) {
    SType next_stype = find_next_stype(stype, stype0);
    if (stype == next_stype) {
      switch (stype) {
        case SType::BOOL:    force_as_bool(inputcol, membuf); break;
        case SType::INT8:    force_as_int<int8_t>(inputcol, membuf); break;
        case SType::INT16:   force_as_int<int16_t>(inputcol, membuf); break;
        case SType::INT32:   force_as_int<int32_t>(inputcol, membuf); break;
        case SType::INT64:   force_as_int<int64_t>(inputcol, membuf); break;
        case SType::FLOAT32: force_as_real<float>(inputcol, membuf); break;
        case SType::FLOAT64: force_as_real<double>(inputcol, membuf); break;
        case SType::STR32:   force_as_str<uint32_t>(inputcol, membuf, strbuf); break;
        case SType::STR64:   force_as_str<uint64_t>(inputcol, membuf, strbuf); break;
        case SType::OBJ:     parse_as_pyobj(inputcol, membuf); break;
        default:
          throw RuntimeError()
            << "Unable to create Column of type " << stype << " from list";
      }
      break; // while(stype)
    } else {
      bool ret = false;
      switch (stype) {
        case SType::BOOL:    i = parse_as_bool(inputcol, membuf, i); break;
        case SType::INT8:    i = parse_as_int<int8_t>(inputcol, membuf, i); break;
        case SType::INT16:   i = parse_as_int<int16_t>(inputcol, membuf, i); break;
        case SType::INT32:   i = parse_as_int<int32_t>(inputcol, membuf, i); break;
        case SType::INT64:   i = parse_as_int<int64_t>(inputcol, membuf, i); break;
        case SType::FLOAT32: ret = parse_as_real<float>(inputcol, membuf, i); break;
        case SType::FLOAT64: ret = parse_as_real<double>(inputcol, membuf, i); break;
        case SType::STR32:   ret = parse_as_str<uint32_t>(inputcol, membuf, strbuf); break;
        case SType::STR64:   ret = parse_as_str<uint64_t>(inputcol, membuf, strbuf); break;
        case SType::OBJ:     ret = parse_as_pyobj(inputcol, membuf); break;
        default: /* do nothing -- not all STypes are currently implemented. */ break;
      }
      if (i == nrows) break;
      if (ret) break;
      stype = next_stype;
    }
  }
  if (stype == SType::STR32 || stype == SType::STR64) {
    return Column::new_string_column(nrows, std::move(membuf), std::move(strbuf));
  }
  else {
    if (stype == SType::OBJ) {
      membuf.set_pyobjects(/* clear_data = */ false);
    }
    return Column::new_mbuf_column(nrows, stype, std::move(membuf));
  }
}


Column Column::from_pylist(const py::olist& list, int stype0) {
  Column inputcol(new dt::PyList_ColumnImpl(list));
  return resolve_column(inputcol, stype0);
}


Column Column::from_pylist_of_tuples(
    const py::olist& list, size_t index, int stype0)
{
  Column inputcol(new dt::PyTupleList_ColumnImpl(list, index));
  return resolve_column(inputcol, stype0);
}


Column Column::from_pylist_of_dicts(
    const py::olist& list, py::robj name, int stype0)
{
  Column inputcol(new dt::PyDictList_ColumnImpl(list, name));
  return resolve_column(inputcol, stype0);
}




//------------------------------------------------------------------------------
// Create from range
//------------------------------------------------------------------------------

Column Column::from_range(
    int64_t start, int64_t stop, int64_t step, SType stype)
{
  if (stype == SType::STR32 || stype == SType::STR64 ||
      stype == SType::OBJ || stype == SType::BOOL)
  {
    Column col = Column(new dt::Range_ColumnImpl(start, stop, step));
    col.cast_inplace(stype);
    return col;
  }
  return Column(new dt::Range_ColumnImpl(start, stop, step, stype));
}
