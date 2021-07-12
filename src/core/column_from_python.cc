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
Column _parse_date32(const Column&, size_t);
Column _parse_time64(const Column&, size_t);

Error _error(size_t i, py::oobj item, const char* expected_type) {
  return TypeError()
      << "Cannot create column: element at index " << i
      << " is of type " << item.typeobj()
      << ", whereas previous elements were " << expected_type;
}



//------------------------------------------------------------------------------
// Individual parsers
//------------------------------------------------------------------------------
//
// Each parser function has the following signature:
//
//     Column (*parserfn)(const Column& inputcol, size_t i0);
//
// Here `inputcol` is the input column of type obj64, `i0` is the
// index of the first non-None element, and the function is supposed
// to return the parsed column (or throw an error if the input is
// unparseable).
//
// Some parsers pass the inputcol to another parser which will be
// more suitable to for the given input.
//


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
  * Parses column containing python floats or ints, into a FLOAT64
  * Column. If any `int` value is too large to fit into a double,
  * it will be converted into ±inf.
  */
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
        item.parse_int(outptr) ||   // converts to ±inf if overflows
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


/**
  * Parse a column containing python `date.date` objects, as a date32
  * type. If we encounter `date.datetime` objects in the list, then
  * re-parse as time64 type instead.
  */
Column _parse_date32(const Column& inputcol, size_t i0) {
  size_t n = inputcol.nrows();
  Buffer databuf = Buffer::mem(n * sizeof(int32_t));
  auto outptr = static_cast<int32_t*>(databuf.xptr());

  py::oobj item;
  for (size_t i = 0; i < i0; i++) {
    *outptr++ = dt::GETNA<int32_t>();
  }
  for (size_t i = i0; i < n; i++) {
    inputcol.get_element(i, &item);
    if (item.parse_date_as_date(outptr) || item.parse_none(outptr)) {
      outptr++;
    }
    else if (item.is_datetime()) {
      return _parse_time64(inputcol, i0);
    }
    else {
      throw _error(i, item, "date32");
    }
  }
  return Column::new_mbuf_column(n, dt::SType::DATE32, std::move(databuf));
}


/**
  * Parse a column containing python `date.date` objects, as a date32
  * type. If we encounter `date.datetime` objects in the list, then
  * re-parse as time64 type instead.
  */
Column _parse_time64(const Column& inputcol, size_t i0) {
  size_t n = inputcol.nrows();
  Buffer databuf = Buffer::mem(n * sizeof(int64_t));
  auto outptr = static_cast<int64_t*>(databuf.xptr());

  py::oobj item;
  for (size_t i = 0; i < i0; i++) {
    *outptr++ = dt::GETNA<int64_t>();
  }
  for (size_t i = i0; i < n; i++) {
    inputcol.get_element(i, &item);
    if (item.parse_datetime_as_time(outptr) ||
        item.parse_date_as_time(outptr) ||
        item.parse_none(outptr)) {
      outptr++;
    }
    else {
      throw _error(i, item, "time64");
    }
  }
  return Column::new_mbuf_column(n, dt::SType::TIME64, std::move(databuf));
}




//------------------------------------------------------------------------------
// Main
//------------------------------------------------------------------------------

/**
  * Main parser that calls all other parsers.
  */
Column new_parse_column_auto_type(const Column& inputcol) {
  int npbits;
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
  else if (item0.is_date()) {
    return _parse_date32(inputcol, i0);
  }
  else if (item0.is_datetime()) {
    return _parse_time64(inputcol, i0);
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

  // If the type of elements in the column is not known, raise an error
  throw TypeError() << "Cannot create column from a python list: element "
      "at index " << i0 << " has type " << item0.typeobj()
      << ". If you intended to create an obj64 column, please request "
         "this type explicitly.";
}



Column resolve_column(const Column& inputcol, dt::Type type0) {
  if (type0) {
    Column out = inputcol.cast(type0);
    out.materialize();
    return out;
  }
  else {
    return new_parse_column_auto_type(inputcol);
  }
}




}  // namespace
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
