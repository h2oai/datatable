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
#include "expr/expr.h"
#include "expr/py_by.h"
namespace py {



//------------------------------------------------------------------------------
// oby
//------------------------------------------------------------------------------

oby::oby(const robj& src) : oobj(src) {}
oby::oby(const oobj& src) : oobj(src) {}


oby oby::make(const robj& r) {
  return oby(oby::oby_pyobject::make(r));
}


bool oby::check(PyObject* v) {
  return oby::oby_pyobject::check(v);
}


void oby::init(PyObject* m) {
  oby::oby_pyobject::init_type(m);
}


dt::collist_ptr oby::cols(dt::EvalContext& ctx) const {
  // robj cols = reinterpret_cast<const pyobj*>(v)->cols;
  robj cols = reinterpret_cast<const oby::oby_pyobject*>(v)->get_cols();
  return dt::collist_ptr(new dt::collist(ctx, cols, dt::collist::BY_NODE));
}




//------------------------------------------------------------------------------
// oby_pyobject
//------------------------------------------------------------------------------

static PKArgs args___init__(0, 0, 0, true, false, {}, "__init__", nullptr);

void oby::oby_pyobject::impl_init_type(XTypeMaker& xt) {
  xt.set_class_name("datatable.by");
  xt.set_class_doc("by() clause for use in DT[i, j, ...]");
  xt.set_subclassable(true);

  xt.add(CONSTRUCTOR(&oby::oby_pyobject::m__init__, args___init__));
  xt.add(DESTRUCTOR(&oby::oby_pyobject::m__dealloc__));

  static GSArgs args__cols("_cols");
  xt.add(GETTER(&oby::oby_pyobject::get_cols, args__cols));
}


void oby::oby_pyobject::m__init__(const PKArgs& args) {
  olist colslist(args.num_vararg_args());
  size_t i = 0;
  for (robj arg : args.varargs()) {
    colslist.set(i++, arg);
  }
  cols = std::move(colslist);
}


void oby::oby_pyobject::m__dealloc__() {
  cols = nullptr;  // Releases the stored oobj
}


oobj oby::oby_pyobject::get_cols() const {
  return cols;
}




} // namespace py
