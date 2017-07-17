#ifndef dt_PY_COLUMNSET_H
#define dt_PY_COLUMNSET_H
#include <Python.h>
#include "datatable.h"
#include "rowindex.h"

typedef struct ColumnSet_PyObject {
    PyObject_HEAD
    Column **columns;
} ColumnSet_PyObject;


extern PyTypeObject ColumnSet_PyType;


int columnset_unwrap(PyObject *object, void *address);

PyObject* pycolumns_from_slice(PyObject *self, PyObject *args);
PyObject* pycolumns_from_array(PyObject *self, PyObject *args);
PyObject* pycolumns_from_mixed(PyObject *self, PyObject *args);


int init_py_columnset(PyObject *module);

#endif
