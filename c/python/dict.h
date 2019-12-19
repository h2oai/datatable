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
#ifndef dt_PYTHON_DICT_h
#define dt_PYTHON_DICT_h
#include "python/obj.h"

namespace py {
class dict_iterator;


/**
 * C++ interface to Python dict object.
 *
 * Keys / values are `py::robj`s. This class supports retrieving a value by
 * its key, querying existence of a key, inserting new key/value pair, and
 * iterating over all key/values.
 *
 * This class does not support "NULL" state, in the sense that the internal
 * `.v` pointer is never nullptr. In particular, this means that `bool(odict)`
 * always returns true.
 *
 * Public API
 * ----------
 * size()
 *   Return the number of key-value pairs in the dictionary.
 *
 * empty()
 *   Same as `size() == 0`.
 *
 * has(key)
 *   Return true iff the provided `key` is in the dictionary. If `key` is not
 *   hashable, return false without raising an exception.
 *
 * get(key)
 *   Retrieve the value corresponding to the provided `key`. If the `key` is
 *   not present (or not hashable), return a "NULL" `robj`.
 *
 * get_or_none(key)
 *   Similar to `get(key)`, but if the key is not present, return python None.
 *
 * set(key, val)
 *   Insert `val` into the dictionary under the key `key`. An exception will
 *   be thrown if the key is not hashable.
 *
 * del(key)
 *   Delete the entry with key `key`.
 *
 * begin() .. end()
 *   Return iterators over the dictionary key-value pairs. These methods
 *   provide support for the standard C++11 loops:
 *
 *      for (const std::pair<robj, robj>& kv : dict) { ... }
 *      for (const auto& kv : dict) { ... }
 *      for (const auto& [key, val] : dict) { ... }  // C++17
 *
 */
class odict : public oobj {
  public:
    odict();
    odict(const odict&) = default;
    odict(odict&&) = default;
    odict& operator=(const odict&) = default;
    odict& operator=(odict&&) = default;

    odict copy() const;

    size_t size() const;
    bool empty() const;
    bool has(_obj key) const;
    robj get(_obj key) const;
    robj get_or_none(_obj key) const;
    void set(_obj key, _obj val);
    void del(_obj key);

    dict_iterator begin() const;
    dict_iterator end() const;

  private:
    odict(oobj&&);
    odict(const robj&);
    friend class _obj;
};



/**
 * Similar to `odict`, however the underlying pointer is not owned.
 */
class rdict : public robj {
  public:
    using robj::robj;

    static rdict unchecked(const robj&);

    size_t size() const;
    bool empty() const;
    bool has(_obj key) const;
    robj get(_obj key) const;
    robj get_or_none(_obj key) const;

    dict_iterator begin() const;
    dict_iterator end() const;

  private:
    rdict(const robj&);
    friend class _obj;
};



/**
 * Helper class for iterating over a `py::odict`.
 *
 * The standard constructor takes a dictionary PyObject* `d`, and a "position"
 * argument `i0` which must be either 0 for `begin()`, or -1 for `end()`.
 */
class dict_iterator {
  public:
    using value_type = std::pair<py::robj, py::robj>;
    using category_type = std::input_iterator_tag;

  private:
    py::oobj   iter;
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
