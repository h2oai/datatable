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


class Float {
  protected:
    PyObject* obj;

  public:
    Float();
    Float(PyObject*);
    Float(const Float&);

    template <typename T> T value() const;

    friend void swap(Float& first, Float& second) noexcept;
};


class oFloat : public Float {
  public:
    oFloat();
    oFloat(double x);
    oFloat(PyObject*);
    oFloat(const oFloat&);
    oFloat(oFloat&&);
    static oFloat _from_pyobject_no_checks(PyObject* v);
    ~oFloat();
};



// Explicit instantiation
extern template float  Float::value() const;
extern template double Float::value() const;


}  // namespace py

#endif
