//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#ifndef dt_PYTHON_DICT_h
#define dt_PYTHON_DICT_h
#include <Python.h>
#include "python/obj.h"

namespace py {
class dict_iterator;


class odict : public oobj {
  public:
    using oobj::oobj;
    using iterator = dict_iterator;
    odict();

    obj get(obj key) const;

    iterator begin() const;
    iterator end() const;
};



class dict_iterator {
  public:
    using value_type = std::pair<py::obj, py::obj>;
    using category_type = std::input_iterator_tag;

  private:
    PyObject*  dict;
    Py_ssize_t pos;
    value_type curr_value;

  public:
    dict_iterator(PyObject* d, Py_ssize_t i0);
    dict_iterator(const dict_iterator&) = default;
    dict_iterator& operator=(const dict_iterator&) = default;

    dict_iterator& operator++();
    value_type operator*() const;
    bool operator==(const dict_iterator&) const;
    bool operator!=(const dict_iterator&) const;

  private:
    void advance();
};


}

#endif
