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
#ifndef dt_EXPR_PY_JOIN_h
#define dt_EXPR_PY_JOIN_h
#include "python/obj.h"
#include "python/xobject.h"
namespace py {



class ojoin : public oobj
{
  class pyobj : public XObject<pyobj> {
    public:
      oobj join_frame;

      void m__init__(const PKArgs&);
      void m__dealloc__();
      oobj get_joinframe() const;

      static void impl_init_type(XTypeMaker& xt);

    private:
      // The class does not use traditional constructor/destructor mechanism.
      // Instead, the objects are created/deleted by Python C backend.
      pyobj();
      ~pyobj();
  };

  public:
    ojoin() = default;
    ojoin(const ojoin&) = default;
    ojoin(ojoin&&) = default;
    ojoin& operator=(const ojoin&) = default;
    ojoin& operator=(ojoin&&) = default;

    DataTable* get_datatable() const;

    static bool check(PyObject* v);
    static void init(PyObject* m);

  private:
    friend class _obj;
    ojoin(const robj&);
};



}  // namespace py
#endif
