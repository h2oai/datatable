#ifndef Dt_ROWS_H
#define Dt_ROWS_H
#include <Python.h>

typedef enum dt_RowsIndexKind {
    RI_ARRAY,
    RI_SLICE,
} dt_RowsIndexKind;


typedef struct dt_RowsIndexObject {
    PyObject_HEAD
    dt_RowsIndexKind kind;
    union {
        struct {
            long length;
            long* rows;
        } riArray;
        struct {
            long start;
            long count;
            long step;
        } riSlice;
    };
} dt_RowsIndexObject;

PyTypeObject dt_RowsIndexType;


#define dt_RowsIndex_NEW() ((dt_RowsIndexObject*) \
    PyObject_CallObject((PyObject*) &dt_RowsIndexType, NULL))

PyObject* rows_from_slice(PyObject *self, PyObject *args);
PyObject* rows_from_array(PyObject *self, PyObject *args);


#endif
