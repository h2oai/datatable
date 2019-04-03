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
#ifndef dt_MODELS_PY_VALIDATOR_h
#define dt_MODELS_PY_VALIDATOR_h
#include "python/obj.h"
#include "python/arg.h"
#include "column.h"

namespace py {
namespace Validator {

struct error_manager {
  error_manager() = default;
  error_manager(const error_manager&) = default;
  Error error_not_positive (PyObject*, const std::string&) const;
  Error error_negative     (PyObject*, const std::string&) const;
  template <typename T>
  Error error_larger_or_equal (PyObject*, const std::string&, T value_max) const;
};

static std::string _name = "Value";
static error_manager _em;


template <typename T>
Error py::Validator::error_manager::error_larger_or_equal(PyObject* src,
                                                   const std::string& name,
                                                   T value_max
) const {
  return ValueError() << name << " should be less than " << value_max << ", got: "
                      << src;
}


// py::_obj validators

/*
*  Less than check.
*/
template <typename T>
void check_less_than(const T value,
                     const T value_max,
                     const py::_obj& o,
                     const std::string& name = _name,
                     error_manager& em = _em)
{
  if (!std::isinf(value) && value < value_max) return;
  throw em.error_larger_or_equal<T>(o.to_borrowed_ref(), name, value_max);
}


/*
*  Positive check. Will emit an error, when `value` is not positive, `NaN`
*  or infinity.
*/
template <typename T>
void check_positive(T value,
                    const py::_obj& o,
                    const std::string& name = _name,
                    error_manager& em = _em)
{
  if (!std::isinf(value) && value > 0) return;
  throw em.error_not_positive(o.to_borrowed_ref(), name);
}


/*
*  Not negative check. Will emit an error, when `value` is negative, `NaN`
*  or infinity.
*/
template <typename T>
void check_not_negative(T value,
                        const py::_obj& o,
                        const std::string& name = _name,
                        error_manager& em = _em)
{
  if (!std::isinf(value) && value >= 0) return;
  throw em.error_negative(o.to_borrowed_ref(), name);
}


// py::Arg validators

template <typename T>
void check_positive(T value, const py::Arg& arg, error_manager& em = _em) {
  check_positive<T>(value, arg.to_pyobj(), arg.name(), em);
}


template <typename T>
void check_not_negative(T value, const py::Arg& arg, error_manager& em = _em) {
  check_not_negative<T>(value, arg.to_pyobj(), arg.name(), em);
}


template <typename T>
void check_less_than(T value, T value_max, const py::Arg& arg, error_manager& em = _em) {
  check_less_than<T>(value, value_max, arg.to_pyobj(), arg.name(), em);
}


template<typename T>
bool has_negatives(const Column* col) {
  auto d_n = static_cast<const T*>(col->data());
  for (size_t i = 0; i < col->nrows; ++i) {
    if (d_n[i] < 0) return true;
  }
  return false;
}


} // namespace Validator
} // namespace py

#endif
