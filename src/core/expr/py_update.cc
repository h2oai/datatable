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
#include "expr/py_update.h"
namespace py {



//------------------------------------------------------------------------------
// oupdate_pyobject
//------------------------------------------------------------------------------

static PKArgs args___init__(0, 0, 0, false, true, {}, "__init__", nullptr);


void oupdate::oupdate_pyobject::impl_init_type(XTypeMaker& xt) {
  xt.set_class_name("datatable.update");
  xt.set_class_doc("update() clause for use in DT[i, j, ...]");
  xt.set_subclassable(false);

  xt.add(CONSTRUCTOR(&oupdate::oupdate_pyobject::m__init__, args___init__));
  xt.add(DESTRUCTOR(&oupdate::oupdate_pyobject::m__dealloc__));

  static GSArgs args__names("_names");
  static GSArgs args__exprs("_exprs");
  xt.add(GETTER(&oupdate::oupdate_pyobject::get_names, args__names));
  xt.add(GETTER(&oupdate::oupdate_pyobject::get_exprs, args__exprs));
}


void oupdate::oupdate_pyobject::m__init__(const PKArgs& args) {
  size_t n = args.num_varkwd_args();
  names_ = py::olist(n);
  exprs_ = py::olist(n);
  size_t i = 0;
  for (auto kw : args.varkwds()) {
    names_.set(i, kw.first);
    exprs_.set(i, kw.second);
    i++;
  }
}


void oupdate::oupdate_pyobject::m__dealloc__() {
  names_ = py::olist();
  exprs_ = py::olist();
}


oobj oupdate::oupdate_pyobject::get_names() const {
  return names_;
}

oobj oupdate::oupdate_pyobject::get_exprs() const {
  return exprs_;
}




//------------------------------------------------------------------------------
// oupdate
//------------------------------------------------------------------------------

oupdate::oupdate(const robj& r) : oobj(r) {
  xassert(check(v));
}


bool oupdate::check(PyObject* v) {
  return oupdate::oupdate_pyobject::check(v);
}


void oupdate::init(PyObject* m) {
  oupdate::oupdate_pyobject::init_type(m);
}


oobj oupdate::get_names() const {
  return reinterpret_cast<const oupdate::oupdate_pyobject*>(v)->get_names();
}

oobj oupdate::get_exprs() const {
  return reinterpret_cast<const oupdate::oupdate_pyobject*>(v)->get_exprs();
}




}  // namespace py
