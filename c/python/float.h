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
#include "utils/pyobj.h"



class PyyFloat {
  private:
    PyObject* obj;

  public:
    PyyFloat();
    PyyFloat(double x);
    PyyFloat(PyObject*);
    PyyFloat(const PyyFloat&);
    PyyFloat(PyyFloat&&);
    ~PyyFloat();

    double value() const;

    static PyyFloat fromAnyObject(PyObject*);
    friend void swap(PyyFloat& first, PyyFloat& second) noexcept;
};


#endif
