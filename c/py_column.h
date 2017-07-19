#ifndef dt_PYCOLUMN_H
#define dt_PYCOLUMN_H
#include <Python.h>
#include "column.h"


typedef struct Column_PyObject {
    PyObject_HEAD
    Column *ref;
} Column_PyObject;


extern PyTypeObject Column_PyType;


#define Column_PyNew() ((Column_PyObject*) \
        PyObject_CallObject((PyObject*) &Column_PyType, NULL))

Column_PyObject* pyColumn_from_Column(Column *col);

int init_py_column(PyObject *module);
void free_xbuf_column(Column *col);

#endif
