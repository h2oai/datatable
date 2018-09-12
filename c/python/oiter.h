//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#ifndef dt_PYTHON_OITER_h
#define dt_PYTHON_OITER_h
#include <Python.h>
#include "python/obj.h"

namespace py {
class iter_iterator;


/**
 * Python iterator interface (the result of `iter(obj)`).
 */
class oiter : public oobj {
  public:
    oiter();
    oiter(const oiter&);
    oiter(oiter&&);
    oiter& operator=(const oiter&);
    oiter& operator=(oiter&&);

    // Returns size of the iterator, or -1 if unknown
    size_t size() const;

    iter_iterator begin() const;
    iter_iterator end() const;

  private:
    // Wrap an existing PyObject* into an `oiter`.
    oiter(PyObject* src);

    friend class _obj;
};


class iter_iterator {
  public:
    using value_type = py::obj;
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
