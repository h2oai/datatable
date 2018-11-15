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


/**
 * Python dict wrapper.
 *
 * Keys / values are `py::robj`s. This class supports retrieving a value by
 * its key, querying existence of a key, inserting new key/value pair, and
 * iterating over all key/values.
 */
class odict : public oobj {
  public:
    odict();
    odict(std::nullptr_t);
    odict(const odict&);
    odict(odict&&);
    odict& operator=(const odict&);
    odict& operator=(odict&&);

    size_t size() const;
    bool has(_obj key) const;
    robj get(_obj key) const;
    void set(_obj key, _obj val);
    void del(_obj key);

    dict_iterator begin() const;
    dict_iterator end() const;

  private:
    odict(PyObject*);
    friend class _obj;
};


class rdict : public robj {
  public:
    using robj::robj;
    size_t size() const;
    robj get(_obj key) const;
    robj get_or_none(_obj key) const;
    dict_iterator begin() const;
    dict_iterator end() const;
};


/**
 * Helper class for iterating over a `py::odict`.
 */
class dict_iterator {
  public:
    using value_type = std::pair<py::robj, py::robj>;
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
