//------------------------------------------------------------------------------
// Copyright 2018-2019 H2O.ai
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
#include <type_traits>
#include "utils/exceptions.h"
#include "write/value_reader.h"
namespace dt {
namespace write {


//------------------------------------------------------------------------------
// view_column_reader
//------------------------------------------------------------------------------

class view_column_reader : public value_reader {
  RowIndex rowindex;
  value_reader_ptr basecol;

  public:
    view_column_reader(const RowIndex& ri, value_reader_ptr&& parent)
      : rowindex(ri), basecol(std::move(parent)) {}

    bool read(writing_context& ctx, size_t row) const override {
      size_t j = rowindex[row];
      return basecol->read(ctx, j);
    }
};



//------------------------------------------------------------------------------
// fwcol_reader
//------------------------------------------------------------------------------

template <typename T>
class fwcol_reader : public value_reader {
  const T* data;

  public:
    fwcol_reader(const Column* col) {
      data = reinterpret_cast<const T*>(col->data());
    }

    bool read(writing_context& ctx, size_t row) const override {
      T value = data[row];
      if (ISNA<T>(value)) return false;
      if (std::is_same<T, int8_t>::value ||
          std::is_same<T, int16_t>::value ||
          std::is_same<T, int32_t>::value) {
        ctx.value_i32 = static_cast<int32_t>(value);
      }
      if (std::is_same<T, int64_t>::value) {
        ctx.value_i64 = static_cast<int64_t>(value);
      }
      if (std::is_same<T, float>::value) {
        ctx.value_f32 = static_cast<float>(value);
      }
      if (std::is_same<T, double>::value) {
        ctx.value_f64 = static_cast<double>(value);
      }
      return true;
    }
};



//------------------------------------------------------------------------------
// strcol_reader
//------------------------------------------------------------------------------

template <typename T>
class strcol_reader : public value_reader {
  const T* offsets;
  const char* strdata;

  public:
    strcol_reader(const Column* col) {
      auto scol = reinterpret_cast<const StringColumn<T>*>(col);
      offsets = scol->offsets();
      strdata = scol->strdata();
    }

    bool read(writing_context& ctx, size_t row) const override {
      T offset0 = offsets[row - 1] & ~GETNA<T>();
      T offset1 = offsets[row];
      if (ISNA<T>(offset1)) return false;
      ctx.value_str.ch = strdata + offset0;
      ctx.value_str.size = static_cast<int64_t>(offset1 - offset0);
      return true;
    }
};



//------------------------------------------------------------------------------
// Base value_reader
//------------------------------------------------------------------------------

value_reader::~value_reader() {
}


value_reader_ptr value_reader::create(const Column* col) {
  using vptr = value_reader_ptr;
  vptr res;
  switch (col->stype()) {
    case SType::BOOL:
    case SType::INT8:    res = vptr(new fwcol_reader<int8_t>(col)); break;
    case SType::INT16:   res = vptr(new fwcol_reader<int16_t>(col)); break;
    case SType::INT32:   res = vptr(new fwcol_reader<int32_t>(col)); break;
    case SType::INT64:   res = vptr(new fwcol_reader<int64_t>(col)); break;
    case SType::FLOAT32: res = vptr(new fwcol_reader<float>(col)); break;
    case SType::FLOAT64: res = vptr(new fwcol_reader<double>(col)); break;
    case SType::STR32:   res = vptr(new strcol_reader<uint32_t>(col)); break;
    case SType::STR64:   res = vptr(new strcol_reader<uint64_t>(col)); break;
    default:
      throw ValueError() << "Cannot read values of stype " << col->stype();
  }

  const RowIndex& ri = col->rowindex();
  if (ri) {
    res = vptr(new view_column_reader(ri, std::move(res)));
  }
  return res;
}



}}  // namespace dt::write
