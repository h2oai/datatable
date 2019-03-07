//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#ifndef dt_PY_DATATABLE_h
#define dt_PY_DATATABLE_h
#include <Python.h>
#include "datatable.h"
#include "options.h"
#include "py_utils.h"
namespace py { class Frame; }

#define BASECLS pydatatable::obj
#define CLSNAME DataTable
#define HOMEFLAG dt_PY_DATATABLE_cc
namespace pydatatable
{

/**
 * Pythonic "handle" to a DataTable object.
 *
 * This object provides an interface through which Python can access the
 * underlying C DataTable (without encumbering the latter with Python-specific
 * elements).
 *
 * Parameters
 * ----------
 * ref
 *     Reference to the C `DataTable` object. The referenced object *is owned*,
 *     that is current object is responsible for deallocating the referenced
 *     datatable upon being garbage-collected.
 *
 */
struct obj : public PyObject {
  DataTable* ref;
  py::Frame* _frame;
};

extern PyTypeObject type;
extern PyBufferProcs as_buffer;


// Internal helper functions
PyObject* wrap(DataTable* dt);
int unwrap(PyObject* source, DataTable** address);
int static_init(PyObject* module);


//---- Generic info ------------------------------------------------------------

DECLARE_INFO(
  datatable.core.DataTable,
  "C-side DataTable object.")

DECLARE_DESTRUCTOR()



};
#undef BASECLS
#undef CLSNAME
#undef HOMEFLAG
#endif
