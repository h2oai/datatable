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
#ifndef dt_EXTRAS_PY_VALIDATOR_h
#define dt_EXTRAS_PY_VALIDATOR_h
#include "python/obj.h"
#include "python/arg.h"

namespace py {
namespace Validator {

struct error_manager {
  error_manager() = default;
  error_manager(const error_manager&) = default;
  Error error_not_positive (PyObject*, const std::string&) const;
  Error error_negative     (PyObject*, const std::string&) const;
};

static std::string _name = "Value";
static error_manager _em;


// py::_obj validators

/*
*  Positive check. Will emit an error, when `value` is not positive or `NaN`.
*/
template <typename T>
void check_positive(T value,
                    const py::_obj& o,
                    const std::string& name = _name,
                    error_manager& em = _em)
{
  if (value > 0) return;
  throw em.error_not_positive(o.to_borrowed_ref(), name);
}


/*
*  Not negative check. Will emit an error, when `value` is negative or `NaN`.
*/
template <typename T>
void check_not_negative(T value,
                        const py::_obj& o,
                        const std::string& name = _name,
                        error_manager& em = _em)
{
  if (value >= 0) return;
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


} // namespace Validator
} // namespace py

#endif
