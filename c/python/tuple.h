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
 * Class representing a pythonic tuple of elements.
 *
 * This can be either a tuple passed down from Python, or a new tuple object.
 * If `otuple` is created from an existing PyObject, its elements must not be
 * modified. On the contrary, if a tuple is created from scratch, its elements
 * must be properly initialized before the tuple can be returned to Python.
 */
class otuple : public oobj {
  public:
    using oobj::oobj;
    otuple(int n);
    otuple(size_t n);
    otuple(int64_t n);

    size_t size() const;
    obj  get(size_t i) const;
    void set(size_t i, const _obj& value);
    void set(size_t i, oobj&& value);
};


}  // namespace py

#endif
