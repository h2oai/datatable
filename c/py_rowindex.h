//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#ifndef dt_PY_ROWINDEX_h
#define dt_PY_ROWINDEX_h
#include <Python.h>
#include "rowindex.h"
#include "py_utils.h"


#define BASECLS pyrowindex::obj
#define HOMEFLAG dt_PY_ROWINDEX_cc
namespace pyrowindex
{

/**
 * Pythonic handle to a `RowIndex` object.
 *
 * The payload RowIndex is stored as a pointer rather than by value - only
 * because `pyrowindex::obj` is created/managed/destroyed from Python C,
 * circumventing traditional object construction/destruction.
 */
struct obj : public PyObject {
  RowIndex* ref;
};

extern PyTypeObject type;

// Internal helper functions
int static_init(PyObject* module);
PyObject* wrap(const RowIndex& src);



//---- Generic info ------------------------------------------------------------

DECLARE_INFO(
  datatable.core.RowIndex,
  "C-side RowIndex object.")



//---- Getters/setters ---------------------------------------------------------

DECLARE_GETTER(
  nrows,
  "Number of rows in the rowindex")

DECLARE_GETTER(
  min,
  "Smallest value in the rowindex")

DECLARE_GETTER(
  max,
  "Largest value in the rowindex")



//---- Methods -----------------------------------------------------------------

DECLARE_METHOD(
  tolist,
  "tolist()\n\n"
  "Return RowIndex's indices as a list of integers.")

DECLARE_METHOD(
  uplift,
  "uplift(parent_rowindex)\n\n"
  "Returns a new RowIndex which is a result of applying this rowindex to the\n"
  "parent rowindex.")


//---- Python API --------------------------------------------------------------

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
  rowindex_from_filterfn,
  "rowindex_from_filterfn(fptr, nrows)\n\n"
  "Construct a RowIndex object given a pointer to a filtering function and\n"
  "the number of rows that has to be filtered. ",
  HOMEFLAG)


};  // namespace pyrowindex
#undef HOMEFLAG
#undef BASECLS
#endif
