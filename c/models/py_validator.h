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


// Error manager

struct error_manager {
  error_manager() = default;
  error_manager(const error_manager&) = default;
  Error error_is_infinite  (PyObject*, const std::string&) const;
  Error error_not_positive (PyObject*, const std::string&) const;
  Error error_negative     (PyObject*, const std::string&) const;
  template <typename T>
  Error error_greater_than (PyObject*, const std::string&, T value_max) const;
};

static std::string _name = "Value";
static error_manager _em;



template <typename T>
Error py::Validator::error_manager::error_greater_than(PyObject* src,
                                                       const std::string& name,
                                                       T value_max
) const {
  return ValueError() << name << " should be less than or equal to " << value_max
                      << ", got: " << src;
}


// Column validators.

bool has_negatives(const Column& col);


// py::Arg validators

/**
 *  Infinity check.
 */
template <typename T>
void check_finite(T value, const py::Arg& arg, error_manager& em = _em) {
  if (!std::isinf(value)) return;

  py::oobj py_obj = arg.to_robj();
  throw em.error_is_infinite(py_obj.to_borrowed_ref(), arg.name());
}


/**
 *  Positive check. Will emit an error, when `value` is not positive or `NaN`.
 */
template <typename T>
void check_positive(T value, const py::Arg& arg, error_manager& em = _em) {
  if (value > 0) return;

  py::oobj py_obj = arg.to_robj();
  throw em.error_not_positive(py_obj.to_borrowed_ref(), arg.name());
}


/**
 *  Not negative check. Will emit an error, when `value` is negative or `NaN`.
 */
template <typename T>
void check_not_negative(T value, const py::Arg& arg, error_manager& em = _em) {
  if (value >= 0) return;

  py::oobj py_obj = arg.to_robj();
  throw em.error_negative(py_obj.to_borrowed_ref(), arg.name());
}


template <typename T>
void check_less_than_or_equal_to(T value, T value_max, const py::Arg& arg, error_manager& em = _em) {
  if (value <= value_max) return;

  py::oobj py_obj = arg.to_robj();
  throw em.error_greater_than(py_obj.to_borrowed_ref(), arg.name(), value_max);
}


} // namespace Validator
} // namespace py

#endif
