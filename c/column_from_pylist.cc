//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#include "column.h"
#include <cstdlib>         // std::abs
#include <limits>          // std::numeric_limits
#include <type_traits>     // std::is_same
#include "py_types.h"      // PyLong_AsInt64AndOverflow
#include "python/_all.h"
#include "python/list.h"   // py::olist
#include "python/string.h" // py::ostring
#include "utils.h"
#include "utils/exceptions.h"

//------------------------------------------------------------------------------
// Helper iterator classes
//------------------------------------------------------------------------------

class iterable {
  public:
    virtual ~iterable();
    virtual size_t size() const = 0;
    virtual py::robj item(size_t i) const = 0;
};


class ilist : public iterable {
  private:
    const py::olist& list;

  public:
    explicit ilist(const py::olist& src);
    size_t size() const override;
    py::robj item(size_t i) const override;
};


class ituplist : public iterable {
  private:
    const py::olist& tuple_list;
    const size_t j;

  public:
    ituplist(const py::olist& src, size_t index);
    size_t size() const override;
    py::robj item(size_t i) const override;
};


class idictlist : public iterable {
  private:
    const py::robj key;
    std::vector<py::rdict> dict_list;

  public:
    idictlist(const py::olist& src, py::robj name);
    size_t size() const override;
    py::robj item(size_t i) const override;
  };


//------------------------------------------------------------------------------

iterable::~iterable() {}


ilist::ilist(const py::olist& src) : list(src) {}

size_t ilist::size() const { return list.size(); }

py::robj ilist::item(size_t i) const { return list[i]; }


ituplist::ituplist(const py::olist& src, size_t index)
    : tuple_list(src), j(index) {}

size_t ituplist::size() const { return tuple_list.size(); }

py::robj ituplist::item(size_t i) const {
  return py::rtuple::unchecked(tuple_list[i])[j];
}


idictlist::idictlist(const py::olist& src, py::robj name) : key(name) {
  for (size_t i = 0; i < src.size(); ++i) {
    dict_list.push_back(src[i].to_rdict());
  }
}

size_t idictlist::size() const {
  return dict_list.size();
}

py::robj idictlist::item(size_t i) const {
  return dict_list[i].get_or_none(key);
}



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
static bool parse_as_bool(const iterable* list, MemoryRange& membuf, size_t& from)
{
  size_t nrows = list->size();
  membuf.resize(nrows);
  int8_t* outdata = static_cast<int8_t*>(membuf.wptr());

  size_t i = 0;
  try {
    for (; i < nrows; ++i) {
      py::robj item = list->item(i);
      // This will throw an exception if the value is not bool-like.
      outdata[i] = item.to_bool();
    }
  } catch (const std::exception&) {
    from = i;
    return false;
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
static void force_as_bool(const iterable* list, MemoryRange& membuf)
{
  size_t nrows = list->size();
  membuf.resize(nrows);
  int8_t* outdata = static_cast<int8_t*>(membuf.wptr());

  for (size_t i = 0; i < nrows; ++i) {
    py::robj item = list->item(i);
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
static bool parse_as_int(const iterable* list, MemoryRange& membuf, size_t& from)
{
  size_t nrows = list->size();
  membuf.resize(nrows * sizeof(T));
  T* outdata = static_cast<T*>(membuf.wptr());

  int overflow = 0;
  for (int j = 0; j < 2; ++j) {
    size_t ifrom = j ? 0 : from;
    size_t ito   = j ? from : nrows;

    for (size_t i = ifrom; i < ito; ++i) {
      py::robj item = list->item(i);

      if (item.is_none()) {
        outdata[i] = GETNA<T>();
        continue;
      }
      if (item.is_int()) {
        py::oint litem = item.to_pyint();
        outdata[i] = litem.ovalue<T>(&overflow);
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
static void force_as_int(const iterable* list, MemoryRange& membuf)
{
  size_t nrows = list->size();
  membuf.resize(nrows * sizeof(T));
  T* outdata = static_cast<T*>(membuf.wptr());

  for (size_t i = 0; i < nrows; ++i) {
    py::robj item = list->item(i);
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
static bool parse_as_double(const iterable* list, MemoryRange& membuf, size_t& from)
{
  size_t nrows = list->size();
  membuf.resize(nrows * sizeof(double));
  double* outdata = static_cast<double*>(membuf.wptr());

  int overflow = 0;
  for (int j = 0; j < 2; ++j) {
    size_t ifrom = j ? 0 : from;
    size_t ito   = j ? from : nrows;
    for (size_t i = ifrom; i < ito; ++i) {
      py::robj item = list->item(i);

      if (item.is_none()) {
        outdata[i] = GETNA<double>();
        continue;
      }
      if (item.is_int()) {
        py::oint litem = item.to_pyint();
        outdata[i] = litem.ovalue<double>(&overflow);
        continue;
      }
      if (item.is_float()) {
        outdata[i] = item.to_double();
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
static void force_as_real(const iterable* list, MemoryRange& membuf)
{
  size_t nrows = list->size();
  membuf.resize(nrows * sizeof(T));
  T* outdata = static_cast<T*>(membuf.wptr());

  int overflow = 0;
  for (size_t i = 0; i < nrows; ++i) {
    py::robj item = list->item(i);

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
static bool parse_as_str(const iterable* list, MemoryRange& offbuf,
                         MemoryRange& strbuf)
{
  size_t nrows = list->size();
  offbuf.resize((nrows + 1) * sizeof(T));
  T* offsets = static_cast<T*>(offbuf.wptr()) + 1;
  offsets[-1] = 0;
  if (!strbuf) {
    strbuf.resize(nrows * 4);  // arbitrarily 4 chars per element
  }
  char* strptr = static_cast<char*>(strbuf.xptr());

  T curr_offset = 0;
  size_t i = 0;
  for (i = 0; i < nrows; ++i) {
    py::robj item = list->item(i);

    if (item.is_none()) {
      offsets[i] = curr_offset | GETNA<T>();
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
static void force_as_str(const iterable* list, MemoryRange& offbuf,
                         MemoryRange& strbuf)
{
  size_t nrows = list->size();
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
  for (size_t i = 0; i < nrows; ++i) {
    py::oobj item = list->item(i);

    if (item.is_none()) {
      offsets[i] = curr_offset | GETNA<T>();
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
          offsets[i] = curr_offset | GETNA<T>();
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
      offsets[i] = curr_offset | GETNA<T>();
    }
  }
  strbuf.resize(curr_offset);
}



//------------------------------------------------------------------------------
// Object
//------------------------------------------------------------------------------

static bool parse_as_pyobj(const iterable* list, MemoryRange& membuf)
{
  size_t nrows = list->size();
  membuf.resize(nrows * sizeof(PyObject*));
  PyObject** outdata = static_cast<PyObject**>(membuf.wptr());

  for (size_t i = 0; i < nrows; ++i) {
    py::oobj item = list->item(i);
    if (item.is_float() && std::isnan(item.to_double())) {
      outdata[i] = py::None().release();
    } else {
      outdata[i] = std::move(item).release();
    }
  }
  return true;
}


// No "force" method, because `parse_as_pyobj()` is already capable of
// processing any pylist.



//------------------------------------------------------------------------------
// Parse controller
//------------------------------------------------------------------------------

static int find_next_stype(int curr_stype, int stype0) {
  if (stype0 > 0) {
    return stype0;
  }
  if (stype0 < 0) {
    return std::min(curr_stype + 1, -stype0);
  }
  if (curr_stype == DT_STYPES_COUNT - 1) {
    return curr_stype;
  }
  return (curr_stype + 1) % int(DT_STYPES_COUNT);
}



Column* Column::from_py_iterable(const iterable* il, int stype0)
{
  MemoryRange membuf;
  MemoryRange strbuf;
  // TODO: Perhaps `stype` and `curr_stype` should have type SType ?
  int stype = find_next_stype(0, stype0);
  size_t i = 0;
  while (stype) {
    int next_stype = find_next_stype(stype, stype0);
    if (stype == next_stype) {
      switch (static_cast<SType>(stype)) {
        case SType::BOOL:    force_as_bool(il, membuf); break;
        case SType::INT8:    force_as_int<int8_t>(il, membuf); break;
        case SType::INT16:   force_as_int<int16_t>(il, membuf); break;
        case SType::INT32:   force_as_int<int32_t>(il, membuf); break;
        case SType::INT64:   force_as_int<int64_t>(il, membuf); break;
        case SType::FLOAT32: force_as_real<float>(il, membuf); break;
        case SType::FLOAT64: force_as_real<double>(il, membuf); break;
        case SType::STR32:   force_as_str<uint32_t>(il, membuf, strbuf); break;
        case SType::STR64:   force_as_str<uint64_t>(il, membuf, strbuf); break;
        case SType::OBJ:     parse_as_pyobj(il, membuf); break;
        default:
          throw RuntimeError()
            << "Unable to create Column of type " << stype << " from list";
      }
      break; // while(stype)
    } else {
      bool ret = false;
      switch (static_cast<SType>(stype)) {
        case SType::BOOL:    ret = parse_as_bool(il, membuf, i); break;
        case SType::INT8:    ret = parse_as_int<int8_t>(il, membuf, i); break;
        case SType::INT16:   ret = parse_as_int<int16_t>(il, membuf, i); break;
        case SType::INT32:   ret = parse_as_int<int32_t>(il, membuf, i); break;
        case SType::INT64:   ret = parse_as_int<int64_t>(il, membuf, i); break;
        case SType::FLOAT64: ret = parse_as_double(il, membuf, i); break;
        case SType::STR32:   ret = parse_as_str<uint32_t>(il, membuf, strbuf); break;
        case SType::STR64:   ret = parse_as_str<uint64_t>(il, membuf, strbuf); break;
        case SType::OBJ:     ret = parse_as_pyobj(il, membuf); break;
        default: /* do nothing -- not all STypes are currently implemented. */ break;
      }
      if (ret) break;
      stype = next_stype;
    }
  }
  Column* col = Column::new_column(static_cast<SType>(stype));
  if (static_cast<SType>(stype) == SType::OBJ) {
    membuf.set_pyobjects(/* clear_data = */ false);
  }
  if (static_cast<SType>(stype) == SType::STR32 ||
      static_cast<SType>(stype) == SType::STR64)
  {
    col->replace_buffer(std::move(membuf), std::move(strbuf));
  } else {
    col->replace_buffer(std::move(membuf));
  }
  return col;
}


Column* Column::from_pylist(const py::olist& list, int stype0) {
  ilist il(list);
  return from_py_iterable(&il, stype0);
}


Column* Column::from_pylist_of_tuples(
    const py::olist& list, size_t index, int stype0)
{
  ituplist il(list, index);
  return from_py_iterable(&il, stype0);
}


Column* Column::from_pylist_of_dicts(
    const py::olist& list, py::robj name, int stype0)
{
  idictlist il(list, name);
  return from_py_iterable(&il, stype0);
}




//------------------------------------------------------------------------------
// Create from range
//------------------------------------------------------------------------------

template <typename T>
static Column* _make_range_column(
    int64_t start, int64_t length, int64_t step, SType stype)
{
  Column* col = Column::new_data_column(stype, static_cast<size_t>(length));
  T* elems = static_cast<T*>(col->data_w());
  for (int64_t i = 0, j = start; i < length; ++i) {
    elems[i] = static_cast<T>(j);
    j += step;
  }
  return col;
}


Column* Column::from_range(
    int64_t start, int64_t stop, int64_t step, SType stype)
{
  int64_t length = (stop - start - (step > 0 ? 1 : -1)) / step + 1;
  if (length < 0) length = 0;
  if (stype == SType::VOID) {
    stype = (start == static_cast<int32_t>(start) &&
             stop == static_cast<int32_t>(stop)) ? SType::INT32 : SType::INT64;
  }
  switch (stype) {
    case SType::INT8:  return _make_range_column<int8_t> (start, length, step, stype);
    case SType::INT16: return _make_range_column<int16_t>(start, length, step, stype);
    case SType::INT32: return _make_range_column<int32_t>(start, length, step, stype);
    case SType::INT64: return _make_range_column<int64_t>(start, length, step, stype);
    default: {
      Column* col = _make_range_column<int64_t>(start, length, step, SType::INT64);
      Column* res = col->cast(stype);
      delete col;
      return res;
    }
  }
}
