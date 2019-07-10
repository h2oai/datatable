//------------------------------------------------------------------------------
// Copyright 2018 H2O.ai
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
#ifndef dt_EXPR_SORT_NODE_h
#define dt_EXPR_SORT_NODE_h
#include <memory>
#include "expr/collist.h"
#include "python/obj.h"
#include "python/xobject.h"
namespace py {


class _sort : public XObject<_sort> {
  private:
    oobj cols;
    friend class osort;

  public:
    void m__init__(const PKArgs&);
    void m__dealloc__();
    oobj get_cols() const;

    static void impl_init_type(XTypeMaker&);
};



class osort : public oobj {
  public:
    osort() = default;
    osort(const osort&) = default;
    osort(osort&&) = default;
    osort& operator=(const osort&) = default;
    osort& operator=(osort&&) = default;

    osort(const otuple& cols);

    static bool check(PyObject* v);
    static void init(PyObject* m);

    dt::collist_ptr cols(dt::workframe&) const;

  private:
    // This private constructor will reinterpret the object `r` as an
    // `osort` object.
    osort(const robj& r);
    osort(const oobj& r);
    friend class _obj;
};



}  // namespace py
#endif
