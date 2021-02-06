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
#include "python/xobject.h"
namespace dt {


/**
  * This class corresponds is python-visible `dt.Type`: it describes
  * a type of a single column.
  *
  * This class uses non-standard initialization mechanism: originally
  * it is defined in python (see "__init__.py"), and then we patch
  * that python class so that its definition would match that of
  * PyType. This approach is used in order to allow the class to
  * carry class attributes `.int32`, `.float64`, etc. Regular
  * extension classes do not support class attributes, nor proper
  * metatypes.
  *
  * Because of this mechanism, the XObject's field `.type` is not
  * used: instead we use a static variable defined in "py_type.cc".
  * This is why methods `init()`, `check()` and `cast_from()` has to
  * be redefined.
  */
class PyType : public py::XObject<PyType> {
  private:
    size_t : 64;  // __dict__
    size_t : 64;  // __weakref__
    Type type_;

  public:
    static py::oobj make(Type);

    void m__init__(const py::PKArgs&);
    void m__dealloc__();
    py::oobj m__repr__() const;
    Type get_type() const { return type_; }

    static py::oobj m__compare__(py::robj, py::robj, int op);

    // Redefine these methods from py::XObject, because this type
    // does not undergo "normal" initialization process.
    static void init();
    static bool check(PyObject*);
    static PyType* cast_from(py::robj obj);
};



}
#endif
