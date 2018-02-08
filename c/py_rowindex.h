//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#ifndef dt_PY_ROWINDEX_H
#define dt_PY_ROWINDEX_H
#include <Python.h>
#include "rowindex.h"
#include "py_utils.h"


#define HOMEFLAG dt_PY_ROWINDEX_cc


/**
 * Pythonic reference to a `RowIndex` object.
 */
struct RowIndex_PyObject : public PyObject {
  RowIndeZ* ref;
};

extern PyTypeObject RowIndex_PyType;


DECLARE_FUNCTION(
  rowindex_from_slice,
  "rowindex_from_slice(start, count, step)\n\n"
  "Construct a RowIndex \"slice\" object given a tuple (start, count, \n"
  "step). Neither first nor last referenced index may be negative.",
  HOMEFLAG)

DECLARE_FUNCTION(
  rowindex_from_slicelist,
  "rowindex_from_slicelist(starts, counts, steps)\n\n"
  "Construct a RowIndex object from a list of tuples (start, count, step)\n"
  "that are given in the form of 3 arrays `starts`, `counts`, and `steps`.\n"
  "The arrays don't have to have the same length, however `starts` cannot\n"
  "be shorter than the others. Missing elements in `counts` and `steps` are\n"
  "assumed to be equal 1.",
  HOMEFLAG)

DECLARE_FUNCTION(
  rowindex_from_array,
  "rowindex_from_array(indices)\n\n"
  "Construct RowIndex object from a list of integer indices.",
  HOMEFLAG)

DECLARE_FUNCTION(
  rowindex_from_column,
  "rowindex_from_column(col)\n\n"
  "Construct a RowIndex object from a Column. The column can be either\n"
  "boolean or integer. A boolean column is treated as a filter, and the\n"
  "RowIndex is constructed with the indices that corresponds to the rows\n"
  "where the column has true values (all false / NA cells are skipped).\n\n"
  "An integer column on the other hand is assumed to provide the indices for\n"
  "the RowIndex directly.",
  HOMEFLAG)

DECLARE_FUNCTION(
  rowindex_uplift,
  "rowindex_uplift(rowindex, frame)\n\n"
  "",
  HOMEFLAG)

PyObject* pyrowindex(const RowIndeZ& src);

PyObject* pyrowindex_from_filterfn(PyObject*, PyObject* args);

int init_py_rowindex(PyObject* module);

#undef HOMEFLAG
#endif
