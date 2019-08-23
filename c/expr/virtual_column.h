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


/**
 * This class is a basic building block in creating lazy evaluation
 * pipelines. This is an abstract base class, the derived classes
 * are expected to implement actual computations.
 *
 * A `virtual_column` is conceptually similar to a regular column:
 * it has an `_stype`, the number of rows `_nrows`, and a way to
 * retrieve its `i`-th element via a set of `compute()` overloads.
 */
class virtual_column {
  private:
    size_t _nrows;
    SType  _stype;
    size_t : 56;

  public:
    virtual_column(size_t nrows, SType stype);
    virtual ~virtual_column();

    size_t nrows() const noexcept;
    SType stype() const noexcept;

    virtual void compute(size_t i, int8_t*  out);
    virtual void compute(size_t i, int16_t* out);
    virtual void compute(size_t i, int32_t* out);
    virtual void compute(size_t i, int64_t* out);
    virtual void compute(size_t i, float*   out);
    virtual void compute(size_t i, double*  out);
    virtual void compute(size_t i, CString* out);

    virtual Column to_column();
};



class vcolptr {
  private:
    virtual_column* vcol;  // owned

  public:
    vcolptr() : vcol(nullptr) {}
    vcolptr(virtual_column* v) : vcol(v) {}
    vcolptr(const vcolptr&) = delete;
    vcolptr& operator=(const vcolptr&) = delete;
    vcolptr(vcolptr&& other) {
      vcol = other.vcol;
      other.vcol = nullptr;
    }
    vcolptr& operator=(vcolptr&& other) {
      delete vcol;
      vcol = other.vcol;
      other.vcol = nullptr;
      return *this;
    }
    ~vcolptr() { delete vcol; }

    virtual_column* operator->() { return vcol; }
    const virtual_column* operator->() const { return vcol; }
};


vcolptr virtualize(Column&& column);

vcolptr cast(vcolptr&& vcol, SType new_stype);


}}  // namespace dt::expr
#endif
