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
#ifndef dt_WRITE_VALUE_WRITER_h
#define dt_WRITE_VALUE_WRITER_h
#include <memory>
#include "write/output_options.h"
#include "write/writing_context.h"
#include "column.h"
#include "types.h"
namespace dt {
namespace write {


class value_writer;
using value_writer_ptr = std::unique_ptr<value_writer>;


class value_writer {
  protected:
    Column column;
    size_t max_output_size;

  public:
    value_writer(const Column& col, size_t n);
    virtual ~value_writer();
    static value_writer_ptr create(const Column& col, const output_options&);

    // Write value `ctx.value_XX` into the output stream `ctx.ch`.
    // Advance the output pointer `ctx.ch` to the new output position.
    // The value to be written is never NA.
    //
    // The output buffer in the writing context is guaranteed to have
    // at least `max_output_size` bytes available starting from the
    // current output position.
    //
    virtual void write_normal(size_t row, writing_context& ctx) const = 0;
    virtual void write_quoted(size_t row, writing_context& ctx) const = 0;

    // Values that are written can generally be of two kinds: either
    // they have an upper limit on the number of characters they take
    // in the output, or there is no such limit. For example, all
    // numeric quantities are of the first kind, whereas strings are
    // of the second.
    //
    // When writing the values of the first kind, it is possible to
    // compute up-front the number of space needed in the output, and
    // to pre-allocate that space. For the values of the second kind,
    // we need to check the available capacity of the output buffer
    // before each write.
    //
    // get_static_output_size() returns the necessary size of the
    // output for the values of the first kind.
    //
    // get_dynamic_output_size() returns the approximate size of the
    // output for the values of the second kind, and 0 for the values
    // of the first kind.
    //
    size_t get_static_output_size() const;
    size_t get_dynamic_output_size() const;
};



}}  // namespace dt::write
#endif
