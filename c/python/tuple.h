//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#ifndef dt_PYTHON_TUPLE_h
#define dt_PYTHON_TUPLE_h
#include <Python.h>
#include "python/obj.h"

namespace py {


/**
 * Python tuple of elements.
 *
 * This can be either a tuple passed down from Python, or a new tuple object.
 * If `otuple` is created from an existing PyObject, its elements must not be
 * modified. On the contrary, if a tuple is created from scratch, its elements
 * must be properly initialized before the tuple can be returned to Python.
 */
class otuple : public oobj {
  public:
    otuple();
    otuple(int n);
    otuple(size_t n);
    otuple(int64_t n);
    otuple(const otuple&);
    otuple(otuple&&);
    otuple& operator=(const otuple&);
    otuple& operator=(otuple&&);

    size_t size() const noexcept;

    obj operator[](int64_t i) const;
    obj operator[](size_t i) const;
    obj operator[](int i) const;

    /**
     * `set(...)` methods should only be used to fill-in a new tuple object.
     * They do not perform any bounds checks; and cannot properly overwrite
     * an existing value.
     */
    void set(int64_t i, const _obj& value);
    void set(size_t i,  const _obj& value);
    void set(int i,     const _obj& value);
    void set(int64_t i, oobj&& value);
    void set(size_t i,  oobj&& value);
    void set(int i,     oobj&& value);

    /**
     * `replace(...)` methods are safer alternatives to `set(...)`: they
     * perform proper bounds checks, and if an element already exists in the
     * tuple at index `i`, it will be properly disposed of.
     */
    void replace(int64_t i, const _obj& value);
    void replace(size_t i,  const _obj& value);
    void replace(int i,     const _obj& value);
    void replace(int64_t i, oobj&& value);
    void replace(size_t i,  oobj&& value);
    void replace(int i,     oobj&& value);
};


/**
 * Same as `otuple`, only the object is owned by reference.
 * The constructor will not check whether the object is created from PyTuple
 * or not -- the caller is expected to do that himself. Thus, this class
 * should be used judiciously.
 */
class rtuple : public obj {
  public:
    using obj::obj;
    size_t size() const noexcept;
    obj operator[](size_t i) const;
};


}  // namespace py

#endif
