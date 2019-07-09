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
#include "py_rowindex.h"
#include "python/_all.h"
#include "python/obj.h"
#include "python/string.h"

namespace py {


//------------------------------------------------------------------------------
// orowindex::pyobject
//------------------------------------------------------------------------------

static PKArgs args___init__(0, 0, 0, false, false, {}, "__init__", nullptr);

void orowindex::pyobject::m__init__(const PKArgs&) {
  ri = nullptr;
}

void orowindex::pyobject::m__dealloc__() {
  delete ri;
  ri = nullptr;
}


oobj orowindex::pyobject::m__repr__() {
  std::ostringstream out;
  out << "datatable.internal.RowIndex(";
  if (ri->isarr32()) out << "int32[" << ri->size() << "]";
  if (ri->isarr64()) out << "int64[" << ri->size() << "]";
  if (ri->isslice()) out << ri->slice_start() << '/' << ri->size() << '/'
                         << static_cast<int64_t>(ri->slice_step());
  out << ")";
  return ostring(out.str());
}



static GSArgs args_type("type");

oobj orowindex::pyobject::get_type() const {
  RowIndexType rt = ri->type();
  return rt == RowIndexType::SLICE? ostring("slice") :
         rt == RowIndexType::ARR32? ostring("arr32") :
         rt == RowIndexType::ARR64? ostring("arr64") : None();
}


static GSArgs args_nrows("nrows");

oobj orowindex::pyobject::get_nrows() const {
  return oint(ri->size());
}


static GSArgs args_min("min");

oobj orowindex::pyobject::get_min() const {
  return oint(ri->min());
}


static GSArgs args_max("max");

oobj orowindex::pyobject::get_max() const {
  return oint(ri->max());
}


static PKArgs args_to_list(
  0, 0, 0, false, false,
  {}, "to_list",

"to_list(self)\n"
"--\n\n"
"Convert this RowIndex into a python list of indices.\n"
);

oobj orowindex::pyobject::to_list(const PKArgs&) {
  size_t n = ri->size();
  olist res(n);
  ri->iterate(0, n, 1,
    [&](size_t i, size_t j) {
      res.set(i, j == RowIndex::NA? None() : oint(j));
    });
  return std::move(res);
}



void orowindex::pyobject::impl_init_type(XTypeMaker& xt) {
  using ori = orowindex::pyobject;
  xt.set_class_name("datatable.internal.RowIndex");
  xt.add(CONSTRUCTOR(&ori::m__init__, args___init__));
  xt.add(DESTRUCTOR(&ori::m__dealloc__));
  xt.add(GETTER(&ori::get_type, args_type));
  xt.add(GETTER(&ori::get_nrows, args_nrows));
  xt.add(GETTER(&ori::get_min, args_min));
  xt.add(GETTER(&ori::get_max, args_max));
  xt.add(METHOD(&ori::to_list, args_to_list));
  xt.add(METHOD__REPR__(&ori::m__repr__));
}




//------------------------------------------------------------------------------
// orowindex
//------------------------------------------------------------------------------

orowindex::orowindex(const RowIndex& rowindex) {
  PyObject* pytype = reinterpret_cast<PyObject*>(&pyobject::type);
  v = PyObject_CallObject(pytype, nullptr);
  if (!v) throw PyError();
  auto p = static_cast<orowindex::pyobject*>(v);
  p->ri = new RowIndex(rowindex);
}


bool orowindex::check(PyObject* v) {
  return pyobject::check(v);
}




}  // namespace py
