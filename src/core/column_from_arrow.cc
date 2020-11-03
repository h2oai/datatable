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
#include "column/arrow_fw.h"
#include "stype.h"
#include "utils/arrow_structs.h"



Column Column::from_arrow(std::shared_ptr<dt::OArrowArray>&& array,
                          const dt::ArrowSchema* schema)
{
  const char* format = schema->format;
  size_t nrows = static_cast<size_t>((*array)->length);
  size_t nullcount = static_cast<size_t>((*array)->null_count);
  auto nbuffers = (*array)->n_buffers;

  switch (format[0]) {
    case 'n': {  // null
      return Column::new_na_column(nrows, dt::SType::VOID);
    }
    case 'b': {  // boolean
      xassert(nbuffers == 2);
      break;
    }
    case 'c':    // int8
    case 'C': {  // uint8
      xassert(nbuffers == 2);
      break;
    }
    case 's':    // int16
    case 'S': {  // uint16
      xassert(nbuffers == 2);
      break;
    }
    case 'i':    // int32
    case 'I': {  // uint32
      xassert(nbuffers == 2);
      break;
    }
    case 'l':    // int64
    case 'L': {  // uint64
      xassert(nbuffers == 2);
      Buffer valid = Buffer::from_arrowarray((*array)->buffers[0], (nrows + 7)/8, array);
      Buffer data = Buffer::from_arrowarray((*array)->buffers[1], nrows*sizeof(int64_t), array);
      return Column(new dt::ArrowFw_ColumnImpl(nrows, dt::SType::INT64,
                                               std::move(valid), std::move(data)));
    }
    case 'f': {  // float32
      xassert(nbuffers == 2);
      break;
    }
    case 'g': {  // float64
      xassert(nbuffers == 2);
      break;
    }
    case 'u': {  // utf-8 string
      xassert(nbuffers == 3);
      break;
    }
    case 'U': {  // large  utf-8 string
      xassert(nbuffers == 3);
      break;
    }
  }
  throw NotImplError()
    << "Cannot create a column from an Arrow array with format `" << format << "`";
}
