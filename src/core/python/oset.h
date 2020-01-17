//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#ifndef dt_PYTHON_OSET_h
#define dt_PYTHON_OSET_h
#include "python/obj.h"
namespace py {


/**
 * Python set() object.
 */
class oset : public oobj {
  public:
    oset();
    oset(const oset&);
    oset(oset&&);
    oset& operator=(const oset&);
    oset& operator=(oset&&);

    size_t size() const;
    bool has(const _obj& key) const;
    void add(const _obj& key);

  private:
    // Wrap an existing PyObject* into an `oset`.
    oset(PyObject* src);

    friend class _obj;
};


}  // namespace py

#endif
