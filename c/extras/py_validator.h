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

class ObjValidator {
  public :
    struct verror_manager {
      verror_manager() = default;
      verror_manager(const verror_manager&) = default;
      virtual ~verror_manager() {}
      virtual Error error_int_not_positive    (PyObject*) const;
      virtual Error error_double_not_positive (PyObject*) const;
      virtual Error error_double_negative     (PyObject*) const;
    };
    static size_t to_size_t_positive(const py::_obj& o,
                                     const _obj::error_manager& em = _em0,
                                     const verror_manager& vm = _vm0);
    static double to_double_positive(const py::_obj& o,
                                     const _obj::error_manager& em = _em0,
                                     const verror_manager& vm = _vm0);
    static double to_double_not_negative(const py::_obj& o,
                                         const _obj::error_manager& em = _em0,
                                         const verror_manager& vm = _vm0);

  protected :
    static verror_manager _vm0;
    static _obj::error_manager _em0;
};


class ArgValidator : ObjValidator::verror_manager {
  private:
    const Arg* arg;
  public:
    ArgValidator(const Arg*);
    size_t to_size_t_positive() const;
    double to_double_positive() const;
    double to_double_not_negative() const;

    virtual Error error_int_not_positive    (PyObject*) const override;
    virtual Error error_double_not_positive (PyObject*) const override;
    virtual Error error_double_negative     (PyObject*) const override;
};

} // namespace py

#endif
