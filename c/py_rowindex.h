#ifndef dt_PY_ROWINDEX_H
#define dt_PY_ROWINDEX_H
#include <Python.h>
#include "rowindex.h"


/**
 * Pythonic reference to a `RowIndex` object.
 *
 * Ownership rules:
 *   - RowIndex_PyObject owns the referenced RowIndex, and is responsible for
 *     its deallocation when RowIndex_PyObject is garbage-collected.
 *   - Any other object may "steal" the reference by setting `ref = NULL`, in
 *     which case they will be responsible for the reference's deallocation.
 */
typedef struct RowIndex_PyObject {
    PyObject_HEAD
    RowIndex *ref;
} RowIndex_PyObject;

PyTypeObject RowIndex_PyType;


#define RowIndex_PyNEW() ((RowIndex_PyObject*) \
    PyObject_CallObject((PyObject*) &RowIndex_PyType, NULL))



RowIndex_PyObject* RowIndexPy_from_slice(PyObject *self, PyObject *args);
RowIndex_PyObject* RowIndexPy_from_array(PyObject *self, PyObject *args);
RowIndex_PyObject* RowIndexPy_from_filter(PyObject *self, PyObject *args);


#endif
