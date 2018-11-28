//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#ifndef dt_PYTHON_ORANGE_h
#define dt_PYTHON_ORANGE_h
#include <Python.h>
#include "python/obj.h"

namespace py {


/**
 * Python range(start, stop, step) object.
 *
 * Bounds that are None are represented as GETNA<int64_t>().
 */
class orange : public oobj {
  public:
    orange(int64_t start, int64_t stop, int64_t step);
    orange(size_t start, size_t stop, size_t step);
    orange(const orange&);
    orange(orange&&);
    orange& operator=(const orange&);
    orange& operator=(orange&&);

    int64_t start() const;
    int64_t stop()  const;
    int64_t step()  const;

  private:
    // Wrap an existing PyObject* into an `orange`. This constructor is
    // private, use `py::org(src).to_pyrange()` instead.
    orange(PyObject* src);

    friend class _obj;
};


}  // namespace py

#endif
