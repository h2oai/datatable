//------------------------------------------------------------------------------
// Copyright 2018-2021 H2O.ai
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
#include "expr/expr.h"
#include "expr/py_by.h"
namespace py {



//------------------------------------------------------------------------------
// oby_pyobject
//------------------------------------------------------------------------------

static PKArgs args___init__(
  0, 0, 1, true, false, {"add_columns"}, "__init__", nullptr
);

void oby::oby_pyobject::m__init__(const PKArgs& args)
{
  const Arg& arg_add_columns = args[0];
  add_columns_ = arg_add_columns.to<bool>(true);

  size_t n = args.num_vararg_args();
  size_t i = 0;
  olist colslist(n);
  for (robj arg : args.varargs()) {
    colslist.set(i++, arg);
  }
  xassert(i == n);
  if (n == 1 && colslist[0].is_list_or_tuple()) {
    cols_ = colslist[0];
  } else {
    cols_ = std::move(colslist);
  }
}


void oby::oby_pyobject::m__dealloc__() {
  cols_ = nullptr;  // Releases the stored oobj
}


oobj oby::oby_pyobject::get_cols() const {
  return cols_;
}

bool oby::oby_pyobject::get_add_columns() const {
  return add_columns_;
}


void oby::oby_pyobject::impl_init_type(XTypeMaker& xt) {
  xt.set_class_name("datatable.by");
  xt.set_class_doc(dt::doc_dt_by);
  xt.set_subclassable(false);

  xt.add(CONSTRUCTOR(&oby::oby_pyobject::m__init__, args___init__));
  xt.add(DESTRUCTOR(&oby::oby_pyobject::m__dealloc__));
}




//------------------------------------------------------------------------------
// oby
//------------------------------------------------------------------------------

oby::oby(const robj& src) : oobj(src) {}
oby::oby(const oobj& src) : oobj(src) {}


oby oby::make(const robj& r) {
  return oby(oby::oby_pyobject::make(r));
}


bool oby::check(PyObject* val) {
  return oby::oby_pyobject::check(val);
}


void oby::init(PyObject* m) {
  oby::oby_pyobject::init_type(m);
}


oobj oby::get_arguments() const {
  return reinterpret_cast<const oby::oby_pyobject*>(v)->get_cols();
}

bool oby::get_add_columns() const {
  return reinterpret_cast<const oby::oby_pyobject*>(v)->get_add_columns();
}




} // namespace py
