//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#ifndef dt_PYTHON_LIST_h
#define dt_PYTHON_LIST_h
#include "python/obj.h"

namespace py {


/**
 * Python list of objects.
 *
 * There are 2 ways of instantiating this class: either by creating a new list
 * of `n` elements via `olist(n)`, or by casting a `py::robj` into a list using
 * `.to_pylist()` method. The former method is used when you want to create a
 * new list and return it to Python, the latter when you're processing a list
 * which came from Python.
 *
 * In most cases when some function/method accepts a pythonic list, it should
 * work just as well when passed a tuple instead. Because of this, `olist`
 * objects can actually wrap both `PyList` objects and `PyTuple` objects --
 * transparently to the user.
 */
class olist : public oobj {
  private:
    bool is_list;
    size_t : 56;

  public:
    olist() = default;
    olist(const olist&) = default;
    olist(olist&&) = default;
    olist& operator=(const olist&) = default;
    olist& operator=(olist&&) = default;

    explicit olist(int n);
    explicit olist(size_t n);
    explicit olist(int64_t n);

    operator bool() const noexcept;
    size_t size() const noexcept;

    robj operator[](size_t i) const;
    robj operator[](int64_t i) const;
    robj operator[](int i) const;

    void set(size_t i,  const _obj& value);
    void set(int64_t i, const _obj& value);
    void set(int i,     const _obj& value);
    void set(size_t i,  oobj&& value);
    void set(int64_t i, oobj&& value);
    void set(int i,     oobj&& value);

    void append(const _obj& value);

  private:
    // Private constructor for ust by py::_obj
    explicit olist(const robj&);

    friend class _obj;
};



class rlist : public robj {
  public:
    static rlist unchecked(const robj&);
    size_t size() const noexcept;
    robj operator[](int64_t i) const;
    robj operator[](size_t i) const;

  private:
    explicit rlist(const robj&);
};




}  // namespace py
#endif
