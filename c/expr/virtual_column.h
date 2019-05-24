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
#ifndef dt_EXPR_VIRTUAL_COLUMN_h
#define dt_EXPR_VIRTUAL_COLUMN_h
#include <memory>
#include "column.h"
#include "types.h"
namespace dt {
namespace expr {

class virtual_column;
using colptr = std::unique_ptr<Column>;
using vcolptr = std::unique_ptr<virtual_column>;


/**
 * This class is a basic building block in creating lazy evaluation
 * pipelines. This is an abstract base class, each `base_expr` is
 * expected to create its own derived class(es) to implement actual
 * computations.
 *
 * A `virtual_column` is conceptually similar to a regular column:
 * it has an `stype()`, the number of rows `nrows()`, and a way to
 * retrieve its `i`-th element via a set of `compute()` overloads.
 */
class virtual_column {
  public:
    virtual ~virtual_column();

    virtual void compute(size_t i, int8_t*  out);
    virtual void compute(size_t i, int16_t* out);
    virtual void compute(size_t i, int32_t* out);
    virtual void compute(size_t i, int64_t* out);
    virtual void compute(size_t i, float*   out);
    virtual void compute(size_t i, double*  out);
    virtual void compute(size_t i, CString* out);

    virtual SType stype() const = 0;
    virtual size_t nrows() const = 0;
    virtual colptr materialize();
};


vcolptr virtualize(colptr&& column);



}}  // namespace dt::expr
#endif
