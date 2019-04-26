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
#ifndef dt_PYTHON_ITER_h
#define dt_PYTHON_ITER_h
#include "python/obj.h"

namespace py {
class iter_iterator;


/**
 * Python iterator interface (the result of `iter(obj)`).
 */
class oiter : public oobj {
  public:
    oiter() = default;
    oiter(const oiter&) = default;
    oiter(oiter&&) = default;
    oiter& operator=(const oiter&) = default;
    oiter& operator=(oiter&&) = default;

    // Returns size of the iterator, or -1 if unknown
    size_t size() const noexcept;

    iter_iterator begin() const noexcept;
    iter_iterator end() const noexcept;

  private:
    // Wrap an existing PyObject* into an `oiter`.
    oiter(PyObject* src);
    friend class _obj;
};



class iter_iterator {
  public:
    using value_type = py::robj;
    using category_type = std::input_iterator_tag;

  private:
    py::oobj iter;
    py::oobj next_value;

  public:
    iter_iterator(PyObject* d);
    iter_iterator(const iter_iterator&) = default;
    iter_iterator& operator=(const iter_iterator&) = default;

    iter_iterator& operator++();
    value_type operator*() const;
    bool operator==(const iter_iterator&) const;
    bool operator!=(const iter_iterator&) const;

  private:
    void advance();
};



}  // namespace py
#endif
