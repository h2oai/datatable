//------------------------------------------------------------------------------
// Copyright 2020-2021 H2O.ai
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
#include "documentation.h"
#include "expr/fexpr_column.h"
#include "expr/namespace.h"
#include "expr/op.h"
#include "python/string.h"
#include "utils/exceptions.h"
namespace py {

static size_t GLOBAL_INDEX = 0;


//------------------------------------------------------------------------------
// __init__()
//------------------------------------------------------------------------------

static PKArgs args___init__(0, 0, 0, false, false, {}, "__init__", nullptr);

void Namespace::m__init__(const PKArgs&) {
  index_ = GLOBAL_INDEX++;
}



//------------------------------------------------------------------------------
// ~Namespace
//------------------------------------------------------------------------------

void Namespace::m__dealloc__() {}



//------------------------------------------------------------------------------
// __repr__()
//------------------------------------------------------------------------------

oobj Namespace::m__repr__() const {
  std::string out = "Namespace(";
  out += std::to_string(index_);
  out += ')';
  return py::ostring(out);
}



//------------------------------------------------------------------------------
// __getattr__()
//------------------------------------------------------------------------------

oobj Namespace::m__getattr__(robj attr) {
  // For system attributes such as `__module__`, `__class__`,
  // `__doc__`, etc, use the standard object.__getattribute__()
  // method.
  if (is_python_system_attr(attr)) {
    return oobj::from_new_reference(
        PyObject_GenericGetAttr(
          reinterpret_cast<PyObject*>(this),
          attr.to_borrowed_ref()
        ));
  }
  return dt::expr::PyFExpr::make(
              new dt::expr::FExpr_ColumnAsAttr(index_, attr));
}



//------------------------------------------------------------------------------
// __getitem__()
//------------------------------------------------------------------------------

oobj Namespace::m__getitem__(robj item) {
  if (!(item.is_int() || item.is_string() ||item.is_slice() ||
        item.is_none() || item.is_pytype() || item.is_stype() ||
        item.is_ltype() || item.is_list_or_tuple()))
  {
    throw TypeError() << "Column selector should be an integer, string, "
                         "or slice, or list/tuple, not " << item.typeobj();
  }
  return dt::expr::PyFExpr::make(
              new dt::expr::FExpr_ColumnAsArg(index_, item));
}



//------------------------------------------------------------------------------
// Init class info
//------------------------------------------------------------------------------

void Namespace::impl_init_type(XTypeMaker& xt) {
  xt.set_class_name("datatable.Namespace");
  xt.set_class_doc(dt::doc_Namespace);
  xt.set_subclassable(false);
  xt.add(CONSTRUCTOR(&Namespace::m__init__, args___init__));
  xt.add(DESTRUCTOR(&Namespace::m__dealloc__));
  xt.add(METHOD__REPR__(&Namespace::m__repr__));
  xt.add(METHOD__GETATTR__(&Namespace::m__getattr__));
  xt.add(METHOD__GETITEM__(&Namespace::m__getitem__));
}




}
