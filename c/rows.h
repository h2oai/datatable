#ifndef Dt_ROWS_H
#define Dt_ROWS_H
#include <Python.h>

typedef enum dt_RowIndexKind {
    RI_ARRAY,
    RI_SLICE,
} dt_RowIndexKind;


typedef struct dt_RowIndexObject {
    PyObject_HEAD
    dt_RowIndexKind kind;
    long length;
    union {
        long* array;
        struct { long start, step; } slice;
    };
} dt_RowIndexObject;

PyTypeObject dt_RowIndexType;


#define dt_RowsIndex_NEW() ((dt_RowIndexObject*) \
    PyObject_CallObject((PyObject*) &dt_RowIndexType, NULL))

PyObject* rows_from_slice(PyObject *self, PyObject *args);
PyObject* rows_from_array(PyObject *self, PyObject *args);


#endif
