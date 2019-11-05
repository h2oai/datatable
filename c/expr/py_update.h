//------------------------------------------------------------------------------
// Copyright 2019 H2O.ai
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
#ifndef dt_EXPR_PY_UPDATE_h
#define dt_EXPR_PY_UPDATE_h
#include "python/obj.h"
#include "python/xobject.h"
namespace py {


class oupdate : public oobj
{
  class oupdate_pyobject : public XObject<oupdate_pyobject> {
    private:
      olist names_;
      olist exprs_;

    private:
      void m__init__(const PKArgs&);
      void m__dealloc__();

    public:
      static void impl_init_type(XTypeMaker& xt);
      oobj get_names() const;
      oobj get_exprs() const;
  };

  public:
    oupdate() = default;
    oupdate(const oupdate&) = default;
    oupdate(oupdate&&) = default;
    oupdate& operator=(const oupdate&) = default;
    oupdate& operator=(oupdate&&) = default;

    static bool check(PyObject* v);
    static void init(PyObject* m);

    oobj get_names() const;
    oobj get_exprs() const;

  private:
    // This private constructor will reinterpret the object `r` as an
    // `oupdate` object. This constructor does not create any new,
    // python objects.
    oupdate(const robj& r);
    friend class _obj;
};



} // namespace py
#endif
