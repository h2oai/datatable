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
#include "models/py_validator.h"

namespace py {
namespace Validator {

// Error messages
Error error_manager::error_not_positive(PyObject* src,
                                        const std::string& name) const
{
  return ValueError() << name << " should be positive: " << src;
}


Error error_manager::error_negative(PyObject* src,
                                    const std::string& name) const
{
  return ValueError() << name << " should be greater than or equal to zero: "
                      << src;
}


Error error_manager::error_is_infinite(PyObject* src,
                                       const std::string& name) const
{
  return ValueError() << name << " should be finite: "
                      << src;
}


/**
 *  Check if column contains any negative values.
 */
bool has_negatives(const Column& col) {
  bool is_valid;
  double min = col.stats()->min_double(&is_valid);
  return is_valid? (min < 0) : true;
}


} // namespace Validator
} // namespace py
