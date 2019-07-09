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
#ifndef dt_PY_ROWINDEX_h
#define dt_PY_ROWINDEX_h
#include "python/obj.h"
#include "python/xobject.h"
#include "rowindex.h"
namespace py {


class orowindex : public oobj {
  public:
    orowindex(const RowIndex& rowindex);
    orowindex() = default;
    orowindex(const orowindex&) = default;
    orowindex(orowindex&&) = default;
    orowindex& operator=(const orowindex&) = default;
    orowindex& operator=(orowindex&&) = default;

    static bool check(PyObject*);


    struct pyobject : public XObject<pyobject> {
      RowIndex* ri;  // owned ref

      void m__init__(const PKArgs&);
      void m__dealloc__();
      oobj m__repr__();
      oobj get_type() const;
      oobj get_nrows() const;
      oobj get_min() const;
      oobj get_max() const;

      oobj to_list(const PKArgs&);

      static void impl_init_type(XTypeMaker&);
    };
};






}  // namespace py
#endif
