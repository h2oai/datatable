//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#ifndef dt_PY_ENCODINGS_H
#define dt_PY_ENCODINGS_H
#include "python/python.h"
#include "encodings.h"


int init_py_encodings(PyObject* module);

int decode_iso8859(const uint8_t* src, int len, uint8_t* dest);
int decode_win1252(const uint8_t* src, int len, uint8_t* dest);
int decode_win1251(const uint8_t* src, int len, uint8_t* dest);


#endif
