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

template <typename T>
class Validator {
  public :
    struct error_manager {
      error_manager() = default;
      error_manager(const error_manager&) = default;
      Error error_not_positive (PyObject*, const std::string&) const;
      Error error_negative     (PyObject*, const std::string&) const;
    };

    // py::_obj validators
    static void check_positive(T value,
                               const py::_obj&,
                               const std::string& name = _name,
                               error_manager& em = _em);
    static void check_not_negative(T value,
                                   const py::_obj&,
                                   const std::string& name = _name,
                                   error_manager& em = _em);

    // py::Arg validators
    static void check_positive(T value,
                               const py::Arg&,
                               error_manager& em = _em);
    static void check_not_negative(T value,
                                   const py::Arg&,
                                   error_manager& em = _em);

  protected :
    static std::string _name;
    static error_manager _em;
};

template <typename T>
typename Validator<T>::error_manager Validator<T>::_em;

template <typename T>
std::string Validator<T>::_name = "Value";

// py::_obj validators
template <typename T>
void Validator<T>::check_positive(T value,
                                  const py::_obj& o,
                                  const std::string& name,
                                  error_manager& em)
{
  if (value <= 0) {
    throw em.error_not_positive(o.v, name);
  }
}

template <typename T>
void Validator<T>::check_not_negative(T value,
                                      const py::_obj& o,
                                      const std::string& name,
                                      error_manager& em)
{
  if (value < 0) {
    throw em.error_negative(o.v, name);
  }
}

// py::Arg validators
template <typename T>
void Validator<T>::check_positive(T value,
                                  const py::Arg& arg,
                                  error_manager& em)
{
  Validator<T>::check_positive(value, arg.pyobj, arg.name(), em);
}

template <typename T>
void Validator<T>::check_not_negative(T value,
                                      const py::Arg& arg,
                                      error_manager& em)
{
  Validator<T>::check_not_negative(value, arg.pyobj, arg.name(), em);
}

// Error messages
template <typename T>
Error Validator<T>::error_manager::error_not_positive(PyObject* src,
                                                      const std::string& name
) const {
  return ValueError() << name << " should be positive: " << src;
}

template <typename T>
Error Validator<T>::error_manager::error_negative(PyObject* src,
                                                  const std::string& name
) const {
  return ValueError() << name << " cannot be negative: " << src;
}

} // namespace py

#endif
