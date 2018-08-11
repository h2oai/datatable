//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#include "py_datatable.h"
#include "column.h"
#include "py_types.h"
#include "py_utils.h"
#include "python/list.h"
#include "python/long.h"
#include "utils/exceptions.h"


/**
 * Construct a new DataTable object from a python list.
 *
 * If the list is empty, then an empty (0 x 0) datatable is produced.
 * If the list is a list of lists, then inner lists are assumed to be the
 * columns, then the number of elements in these lists must be the same
 * and it will be the number of rows in the datatable produced.
 * Otherwise, we assume that the list represents a single data column, and
 * build the datatable appropriately.
 */
PyObject* pydatatable::datatable_from_list(PyObject*, PyObject* args)
{
  PyObject* arg1;
  PyObject* arg2;
  if (!PyArg_ParseTuple(args, "OO:from_list", &arg1, &arg2))
    return nullptr;
  py::list srcs = py::obj(arg1).to_list();
  py::list types = py::obj(arg2).to_list();

  if (srcs && types && srcs.size() != types.size()) {
    throw ValueError() << "The list of sources has size " << srcs.size()
        << ", while the list of types has size " << types.size();
  }

  size_t ncols = srcs.size();
  Column** cols = static_cast<Column**>(std::calloc(ncols + 1, sizeof(void*)));
  if (!cols) throw MemoryError();

  // Check validity of the data and construct the output columnset.
  int64_t nrows = 0;
  for (size_t i = 0; i < ncols; ++i) {
    py::obj item = srcs[i];
    if (item.is_buffer()) {
      cols[i] = Column::from_buffer(item.to_borrowed_ref());
    } else if (item.is_list()) {
      py::list list = item.to_list();
      int stype = 0;
      if (types) {
        PyyLong t = types[i].to_pyint();
        stype = t.value<int32_t>();
        if (ISNA<int32_t>(stype)) stype = 0;
      }
      cols[i] = Column::from_pylist(list.to_pyylist(), stype);
    } else {
      throw ValueError() << "Source list is not list-of-lists";
    }
    if (i == 0) nrows = cols[i]->nrows;
    if (cols[i]->nrows != nrows) {
      throw ValueError() << "Column " << i << " has different number of "
        << "rows (" << cols[i]->nrows << ") than the preceding columns ("
        << nrows << ")";
    }
  }

  return pydatatable::wrap(new DataTable(cols));
}
