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
#ifndef dt_WRITE_VALUE_READER_h
#define dt_WRITE_VALUE_READER_h
#include <memory>
#include "write/writing_context.h"
#include "column.h"
namespace dt {
namespace write {


class value_reader;
using value_reader_ptr = std::unique_ptr<value_reader>;


class value_reader {
  public:
    static value_reader_ptr create(const Column* col);
    virtual ~value_reader();

    // Read value `column[row]` and store it in the writing context
    // as `ctx.value_XX`. Returns true if the value is valid, or
    // false if the value is NA. When false is returned, nothing has
    // to be stored in the writing context.
    //
    // The value stored depends on the stype of the source Column:
    //   BOOL, INT8, INT16, INT32 -> value_i32
    //   INT64 -> value_i64
    //   FLOAT32 -> value_f32
    //   FLOAT64 -> value_f64
    //   STR32, STR64 -> value_str
    //
    virtual bool read(writing_context& ctx, size_t row) const = 0;
};


}} // namespace dt::write
#endif
