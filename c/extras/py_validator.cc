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
#include <extras/py_validator.h>

namespace py {

ObjValidator::verror_manager ObjValidator::_vm0;
_obj::error_manager ObjValidator::_em0;

size_t ObjValidator::to_size_t_positive(const py::_obj& o,
                                     const _obj::error_manager& em,
                                     const verror_manager& vm)
{
  int64_t res = o.to_int64_strict(em);
  if (res <= 0) {
    throw vm.error_int_not_positive(o.v);
  }
  return static_cast<size_t>(res);
}


double ObjValidator::to_double_positive(const py::_obj& o,
                                     const _obj::error_manager& em,
                                     const verror_manager& vm)
{
  double res = o.to_double(em);
  if (res <= 0) {
    throw vm.error_double_not_positive(o.v);
  }
  return res;
}


double ObjValidator::to_double_not_negative(const py::_obj& o,
                                         const _obj::error_manager& em,
                                         const verror_manager& vm)
{
  double res = o.to_double(em);
  if (res < 0) {
    throw vm.error_double_negative(o.v);
  }
  return res;
}


Error ObjValidator::verror_manager::error_int_not_positive(PyObject*) const {
  return ValueError() << "Integer value should be positive";
}


Error ObjValidator::verror_manager::error_double_not_positive(PyObject*) const {
  return ValueError() << "Float value should be positive";
}


Error ObjValidator::verror_manager::error_double_negative(PyObject*) const {
  return ValueError() << "Float value cannot be negative";
}


ArgValidator::ArgValidator(const Arg* arg_in) :
  arg(arg_in)
{
}


size_t ArgValidator::to_size_t_positive() const {
  return ObjValidator::to_size_t_positive(arg->pyobj, *arg, *this);
}


double ArgValidator::to_double_positive() const {
  return ObjValidator::to_double_positive(arg->pyobj, *arg, *this);
}


double ArgValidator::to_double_not_negative() const {
  return ObjValidator::to_double_not_negative(arg->pyobj, *arg, *this);
}


Error ArgValidator::error_int_not_positive(PyObject* src) const {
  return ValueError() << arg->name() << " should be positive: " << src;
}


Error ArgValidator::error_double_not_positive(PyObject* src) const {
  return ValueError() << arg->name() << " should be positive: " << src;
}


Error ArgValidator::error_double_negative(PyObject* src) const {
  return ValueError() << arg->name() << " cannot be negative: " << src;
}

} // namespace py
