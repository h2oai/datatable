#ifndef dt_PY_ROWMAPPING_H
#define dt_PY_ROWMAPPING_H
#include <Python.h>
#include "rowmapping.h"


/**
 * Pythonic reference to a `RowMapping` object.
 *
 * Ownership rules:
 *   - RowMapping_PyObject owns the referenced RowMapping, and is responsible
 *     for its deallocation when RowMapping_PyObject is garbage-collected.
 *   - Any other object may "steal" the reference by setting `ref = NULL`, in
 *     which case they will be responsible for the reference's deallocation.
 */
typedef struct RowMapping_PyObject {
    PyObject_HEAD
    RowMapping *ref;
} RowMapping_PyObject;

extern PyTypeObject RowMapping_PyType;



int rowmapping_unwrap(PyObject *object, void *address);
PyObject* pyrowmapping_from_slice(PyObject*, PyObject *args);
PyObject* pyrowmapping_from_slicelist(PyObject*, PyObject *args);
PyObject* pyrowmapping_from_array(PyObject*, PyObject *args);
PyObject* pyrowmapping_from_column(PyObject*, PyObject *args);
PyObject* pyrowmapping_from_filterfn(PyObject*, PyObject *args);

int init_py_rowmapping(PyObject *module);

#endif
