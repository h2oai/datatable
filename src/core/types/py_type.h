//------------------------------------------------------------------------------
// Copyright 2021 H2O.ai
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
#ifndef dt_TYPES_PY_TYPE_h
#define dt_TYPES_PY_TYPE_h
#include "types/type.h"
#include "python/xargs.h"
#include "python/xobject.h"
namespace dt {


/**
  * This class corresponds to python-visible `dt.Type`: it describes
  * a type of a single column.
  *
  * This class uses "dynamic" initialization mechanism, which may
  * cause some of the class featured to not work normally. In case
  * of any oddities, please check the implementation of XObject
  * class, specifically the `dynamic_type_` property.
  *
  * This mechanism is used because the "standard" extension types do
  * not allow to define class properties (which we need).
  */
class PyType : public py::XObject<PyType, true> {
  private:
    Type type_;

  public:
    static py::oobj make(Type);
    static py::oobj m__compare__(py::robj, py::robj, int op);

    void m__init__(const py::PKArgs&);
    void m__dealloc__();
    py::oobj m__repr__() const;
    size_t m__hash__() const;

    py::oobj get_name() const;
    py::oobj get_min() const;
    py::oobj get_max() const;
    py::oobj list(const py::XArgs&);

    Type get_type() const { return type_; }

    static void impl_init_type(py::XTypeMaker& xt);
};



}
#endif
