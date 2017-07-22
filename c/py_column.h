#ifndef dt_PYCOLUMN_H
#define dt_PYCOLUMN_H
#include <Python.h>
#include "column.h"
#include "py_datatable.h"


typedef struct Column_PyObject {
    PyObject_HEAD
    Column *ref;
    DataTable_PyObject *pydt;
    int64_t colidx;
} Column_PyObject;


extern PyTypeObject Column_PyType;

extern PyObject* pyfn_column_hexview;


#define Column_PyNew() ((Column_PyObject*) \
        PyObject_CallObject((PyObject*) &Column_PyType, NULL))

Column_PyObject* pycolumn_from_column(Column *col, DataTable_PyObject *pydt,
                                      int64_t colidx);

int init_py_column(PyObject *module);
void free_xbuf_column(Column *col);

#endif
