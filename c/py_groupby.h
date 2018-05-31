//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#ifndef dt_PY_GROUPBY_h
#define dt_PY_GROUPBY_h
#include <Python.h>
#include "groupby.h"
#include "py_utils.h"


#define BASECLS pygroupby::obj
#define CLSNAME Groupby
#define HOMEFLAG dt_PY_GROUPBY_cc
namespace pygroupby
{

/**
 * Pythonic handle to a `Groupby` object.
 *
 * The payload Groupby is stored as a pointer rather than by value - only
 * because `pygroupby::obj` is created/managed/destroyed from Python C,
 * circumventing traditional object construction/destruction.
 */
struct obj : public PyObject {
  Groupby* ref;
};

extern PyTypeObject type;

// Internal helper functions
int static_init(PyObject* module);
PyObject* wrap(const Groupby& src);
Groupby* unwrap(PyObject* object);


//---- Generic info ------------------------------------------------------------

DECLARE_INFO(
  datatable.core.Groupby,
  "C-side Groupby object.")

// DECLARE_REPR()
DECLARE_DESTRUCTOR()



//---- Getters/setters ---------------------------------------------------------

DECLARE_GETTER(
  ngroups,
  "Number of groups in the groupby")

DECLARE_GETTER(
  sizes,
  "The array of group sizes")

DECLARE_GETTER(
  offsets,
  "The cumulative array of group sizes in the groupby. The length of the\n"
  "array is `ngroups + 1`, and the first element is always 0.")



};  // namespace pygroupby
#undef HOMEFLAG
#undef BASECLS
#undef CLSNAME
#endif
