#ifndef dt_PY_COLUMNSET_H
#define dt_PY_COLUMNSET_H
#include <Python.h>
#include "datatable.h"
#include "rowmapping.h"


PyObject* pycolumns_from_pymixed(PyObject *self, PyObject *args);


Column** columns_from_pymixed(
    PyObject *elems,
    DataTable *dt,
    RowMapping *rowmapping,
    int (*mapfn)(int64_t row0, int64_t row1, void** out)
);


#endif
