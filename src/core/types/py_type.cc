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
#include "types/py_type.h"
#include "python/_all.h"
namespace dt {


static py::PKArgs args___init__(0, 0, 0, false, false, {}, "__init__", nullptr);


void PyType::m__init__(const py::PKArgs&) {}


void PyType::m__dealloc__() {
  type_ = Type();
}


py::oobj PyType::m__repr__() const {
  return py::ostring("Type.");
}


py::oobj PyType::m__compare__(py::robj x, py::robj y, int op) {

  if (op == Py_EQ) {
    return py::obool();
  }
  return py::False();
}



static const char* doc_Type =
R"(
Type of data in a column.
)";

void PyType::impl_init_type(py::XTypeMaker& xt) {
  xt.set_class_name("datatable.Type");
  xt.set_class_doc(doc_Type);
  xt.set_subclassable(false);
  xt.add(CONSTRUCTOR(&PyType::m__init__, args___init__));
  xt.add(DESTRUCTOR(&PyType::m__dealloc__));
  xt.add(METHOD__REPR__(&PyType::m__repr__));
  xt.add(METHOD__CMP__(&PyType::m__compare__));
}



}
