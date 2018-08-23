//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#ifndef dt_PYTHON_LIST_h
#define dt_PYTHON_LIST_h
#include <Python.h>
#include "python/obj.h"

namespace py {
class Arg;


class list : public oobj {
  private:
    bool is_list;
    size_t : 56;

  public:
    using oobj::oobj;
    list();
    list(PyObject*);
    list(size_t n);

    operator bool() const;
    size_t size() const;
    obj operator[](size_t i) const;

    void set(size_t i,  const _obj& value);
    void set(int64_t i, const _obj& value);
    void set(int i,     const _obj& value);
    void set(size_t i,  oobj&& value);
    void set(int64_t i, oobj&& value);
    void set(int i,     oobj&& value);

    friend class Arg;
};


}  // namespace py

#endif
