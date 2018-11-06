//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#ifndef dt_PY_DATAWINDOW_h
#define dt_PY_DATAWINDOW_h
#include <Python.h>
#include "py_utils.h"


#define BASECLS pydatawindow::obj
#define CLSNAME DataWindow
#define HOMEFLAG dt_PY_DATAWINDOW_cc
namespace pydatawindow
{

/**
 * This object facilitates access to a `DataTable`s data from Python. The idea
 * is that a datatable may be huge, possibly containing gigabytes of data. At
 * the same time, exposing any primitive to a Python runtime requires wrapping
 * that primitive into a `PyObject`, which adds a significant amount of
 * overhead (both in terms of memory and CPU).
 *
 * `DataWindow` objects come to the rescue: they take small rectangular
 * subsections of a datatable's data, and represent them as Python objects.
 * Such limited amount of data is usually sufficient from users' perspective
 * since they are able to view only a limited amount of data in their viewport
 * anyways.
 */
struct obj : public PyObject {
  // Coordinates of the window returned: `row0`..`row1` x `col0`..`col1`.
  // `row0` is the first row to include, `row1` is one after the last. The
  // number of rows in the window is thus `row1 - row0`. Similarly with cols.
  size_t row0, row1;
  size_t col0, col1;

  // List of types (LType) of each column returned. This list will have
  // `col1 - col0` elements.
  PyListObject* types;

  // List of storage types (SType) for each column returned.
  PyListObject* stypes;

  // Actual data within the window, represented as a PyList of PyLists of
  // Python primitives (such as PyLong, PyFloat, etc). The data is returned
  // in column-major order, i.e. each element of the list `data` represents
  // a single column from the parent datatable. The number of elements in
  // this list is `col1 - col0`; and each element is a list of `row1 - row0`
  // items.
  PyListObject* data;

};

extern PyTypeObject type;


//---- Generic info ------------------------------------------------------------

DECLARE_INFO(
  datatable.core.DataWindow,
  "DataWindow object")

DECLARE_CONSTRUCTOR()
DECLARE_DESTRUCTOR()



//---- Getters/setters ---------------------------------------------------------

DECLARE_GETTER(row0, "Starting row index of the data window")
DECLARE_GETTER(row1, "Last row index + 1 of the data window")
DECLARE_GETTER(col0, "Starting column index of the data window")
DECLARE_GETTER(col1, "Last column index + 1 of the data window")
DECLARE_GETTER(types, "LTypes of the columns within the view")
DECLARE_GETTER(stypes, "STypes of the columns within the view")
DECLARE_GETTER(data, "Datatable's data within the specified window")




int static_init(PyObject* module);


};
#undef BASECLS
#undef CLSNAME
#undef HOMEFLAG
#endif
