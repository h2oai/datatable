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
#ifndef dt_EXPR_PY_BY_h
#define dt_EXPR_PY_BY_h
#include "python/obj.h"
#include "python/xobject.h"
namespace py {



class oby : public oobj
{
  class oby_pyobject : public XObject<oby_pyobject> {
    private:
      oobj cols_;
      bool add_columns_;
      size_t : 56;

    public:
      void m__init__(const PKArgs&);
      void m__dealloc__();
      oobj get_cols() const;
      bool get_add_columns() const;

      static void impl_init_type(XTypeMaker& xt);
  };

  public:
    oby() = default;
    oby(const oby&) = default;
    oby(oby&&) = default;
    oby& operator=(const oby&) = default;
    oby& operator=(oby&&) = default;

    // This static constructor is the equivalent of python call `by(r)`.
    // It creates a new `by` object from the column descriptor `r`.
    static oby make(const robj& r);

    static bool check(PyObject* v);
    static void init(PyObject* m);

    oobj get_arguments() const;
    bool get_add_columns() const;

  private:
    // This private constructor will reinterpret the object `r` as an
    // `oby` object. This constructor does not create any new python objects,
    // as opposed to the static `oby::make(r)` constructor.
    oby(const robj& r);
    oby(const oobj&);
    friend class _obj;
};




} // namespace py
#endif
