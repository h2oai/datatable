//------------------------------------------------------------------------------
// Copyright 2020 H2O.ai
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
#include <memory>
#include "column.h"
#include "column/arrow_bool.h"
#include "column/arrow_fw.h"
#include "column/arrow_str.h"
#include "stype.h"
#include "utils/arrow_structs.h"


/**
  * Create a boolean column.
  * (in Arrow, boolean columns use 1 bit per value)
  */
static Column _make_bool(std::shared_ptr<dt::OArrowArray>&& array) {
  xassert((*array)->n_buffers == 2);
  size_t nrows = static_cast<size_t>((*array)->length);
  return Column(new dt::ArrowBool_ColumnImpl(
      nrows,
      Buffer::from_arrowarray((*array)->buffers[0], (nrows + 7)/8, array),
      Buffer::from_arrowarray((*array)->buffers[1], (nrows + 7)/8, array)
  ));
}


/**
  * Create a fixed-width column.
  */
static Column _make_fw(dt::SType stype, std::shared_ptr<dt::OArrowArray>&& array) {
  xassert((*array)->n_buffers == 2);
  size_t nrows = static_cast<size_t>((*array)->length);
  size_t elemsize = stype_elemsize(stype);
  return Column(new dt::ArrowFw_ColumnImpl(
      nrows,
      stype,
      Buffer::from_arrowarray((*array)->buffers[0], (nrows + 7)/8, array),
      Buffer::from_arrowarray((*array)->buffers[1], nrows*elemsize, array)
  ));
}


/**
  * Create a variable-width column.
  */
template <typename T>
static Column _make_vw(dt::SType stype, std::shared_ptr<dt::OArrowArray>&& array) {
  xassert((*array)->n_buffers == 3);
  size_t nrows = static_cast<size_t>((*array)->length);
  size_t datasize = static_cast<const T*>((*array)->buffers[1])[nrows];
  return Column(new dt::ArrowStr_ColumnImpl<T>(
      nrows,
      stype,
      Buffer::from_arrowarray((*array)->buffers[0], (nrows + 7)/8, array),
      Buffer::from_arrowarray((*array)->buffers[1], (nrows + 1)*sizeof(T), array),
      Buffer::from_arrowarray((*array)->buffers[2], datasize, array)
  ));
}


Column Column::from_arrow(std::shared_ptr<dt::OArrowArray>&& array,
                          const dt::ArrowSchema* schema)
{
  const char* format = schema->format;
  size_t nrows = static_cast<size_t>((*array)->length);
  if ((*array)->offset) {
    throw NotImplError() << "Arrow arrays with an offset are not supported";
  }
  // size_t nullcount = static_cast<size_t>((*array)->null_count);

  switch (format[0]) {
    case 'n': {  // null
      return Column::new_na_column(nrows, dt::SType::VOID);
    }
    case 'b': {  // boolean
      return _make_bool(std::move(array));
    }
    case 'c':    // int8
    case 'C': {  // uint8
      return _make_fw(dt::SType::INT8, std::move(array));
    }
    case 's':    // int16
    case 'S': {  // uint16
      return _make_fw(dt::SType::INT16, std::move(array));
    }
    case 'i':    // int32
    case 'I': {  // uint32
      return _make_fw(dt::SType::INT32, std::move(array));
    }
    case 'l':    // int64
    case 'L': {  // uint64
      return _make_fw(dt::SType::INT64, std::move(array));
    }
    case 'f': {  // float32
      return _make_fw(dt::SType::FLOAT32, std::move(array));
    }
    case 'g': {  // float64
      return _make_fw(dt::SType::FLOAT64, std::move(array));
    }
    case 'u': {  // utf-8 string
      return  _make_vw<uint32_t>(dt::SType::STR32, std::move(array));
    }
    case 'U': {  // large  utf-8 string
      return  _make_vw<uint64_t>(dt::SType::STR64, std::move(array));
    }
    case 't': {  // various time formats
      if (format[1] == 'd') {
        if (format[2] == 'D') {  // date32
          return _make_fw(dt::SType::DATE32, std::move(array));
        }
        if (format[2] == 'm') {  // date64
          Column res = _make_fw(dt::SType::INT64, std::move(array));
          res.cast_inplace(dt::SType::DATE32);
          return res;
        }
      }
      break;
    }
  }
  throw NotImplError()
    << "Cannot create a column from an Arrow array with format `" << format << "`";
}
