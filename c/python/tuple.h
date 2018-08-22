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


class otuple : public oobj {
  public:
    using oobj::oobj;
    otuple(size_t n);
    otuple(int64_t n);

    size_t size() const;
    void set(size_t i, const _obj& value);
    void set(size_t i, oobj&& value);
};


}  // namespace py

#endif
