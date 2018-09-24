//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#ifndef dt_PYTHON_FLOAT_h
#define dt_PYTHON_FLOAT_h
#include <Python.h>
#include "python/obj.h"

namespace py {


/**
 * Python float object.
 */
class ofloat : public oobj {
  public:
    ofloat();
    ofloat(double x);
    ofloat(float x);

    ofloat(const ofloat&);
    ofloat(ofloat&&);
    ofloat& operator=(const ofloat&);
    ofloat& operator=(ofloat&&);

    template <typename T> T value() const;

  private:
    ofloat(PyObject*);
    static ofloat from_new_reference(PyObject*);
    friend class _obj;
};



// Explicit instantiation
extern template float  ofloat::value() const;
extern template double ofloat::value() const;


}  // namespace py

#endif
