/**
 *
 *  Make Datatable from a Python list
 *
 */
#include "column.h"
#include "memorybuf.h"
#include "py_datatable.h"
#include "py_types.h"
#include "py_utils.h"


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
PyObject* pydatatable_from_list(UU, PyObject *args)
{
  CATCH_EXCEPTIONS(
    Column **cols = NULL;
    PyObject *list;

    if (!PyArg_ParseTuple(args, "O!:from_list", &PyList_Type, &list))
        return NULL;


    // If the supplied list is empty, return the empty Datatable object
    int64_t listsize = Py_SIZE(list);  // works both for lists and tuples
    if (listsize == 0) {
        dtmalloc(cols, Column*, 1);
        cols[0] = NULL;
        return pydt_from_dt(new DataTable(cols));
    }

    // Basic check validity of the provided data.
    int64_t item0size = Py_SIZE(PyList_GET_ITEM(list, 0));
    for (int64_t i = 0; i < listsize; ++i) {
        PyObject *item = PyList_GET_ITEM(list, i);
        if (!PyList_Check(item)) {
            PyErr_SetString(PyExc_ValueError,
                            "Source list is not list-of-lists");
            goto fail;
        }
        if (Py_SIZE(item) != item0size) {
            PyErr_SetString(PyExc_ValueError,
                            "Source lists have variable number of rows");
            goto fail;
        }
    }

    dtcalloc(cols, Column*, listsize + 1);

    // Fill the data
    for (int64_t i = 0; i < listsize; i++) {
        PyObject *src = PyList_GET_ITEM(list, i);
        cols[i] = Column::from_pylist(src);
    }

    return pydt_from_dt(new DataTable(cols));

  fail:
    if (cols) {
        for (int i = 0; cols[i] != NULL; ++i) {
            delete cols[i];
        }
        dtfree(cols);
    }
    return NULL;
  )
}
