//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#include "py_types.h"
#include "py_utils.h"
#include "column.h"

// TODO: merge with types.h / types.cc


size_t py_buffers_size;




int init_py_types(PyObject*)
{
  init_types();
  py_buffers_size = sizeof(Py_buffer);

  return 1;
}
