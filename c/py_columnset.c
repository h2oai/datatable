#include "columnset.h"
#include "py_columnset.h"
#include "py_utils.h"
#include "utils.h"


PyObject* pycolumns_from_pymixed(PyObject *self, PyObject *args)
{
    PyObject *elems = NULL;

    if (!PyArg_ParseTuple(args, "O!", &PyList_Type, &elems))
        return NULL;

    Column **columns = columns_from_pymixed(elems, NULL, NULL, NULL);
    return NULL;
}



Column** columns_from_pymixed(
    PyObject *elems,
    DataTable *dt,
    RowMapping *rowmapping,
    int (*mapfn)(int64_t row0, int64_t row1, void** out)
) {
    int64_t ncols = PyList_Size(elems);
    int64_t *spec = NULL;

    dtmalloc(spec, int64_t, ncols);
    for (int64_t i = 0; i < ncols; i++) {
        PyObject *elem = PyList_GET_ITEM(elems, i);
        if (PyLong_CheckExact(elem)) {
            spec[i] = (int64_t) PyLong_AsSize_t(elem);
        } else {
            spec[i] = -TOINT64(ATTR(elem, "itype"), 0);
        }
    }
    return columns_from_mixed(spec, ncols, dt, rowmapping, mapfn);

  fail:
    return NULL;
}
