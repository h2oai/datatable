#ifndef dt_PY_COLMAPPING_H
#define dt_PY_COLMAPPING_H
#include <Python.h>
#include "colmapping.h"


/**
 * Pythonic reference to a `ColMapping` object.
 *
 * Ownership rules:
 *   - ColMapping_PyObject owns the referenced ColMapping, and is responsible
 *     for its deallocation when ColMapping_PyObject is garbage-collected.
 *   - Any other object may "steal" the reference by setting `ref = NULL`, in
 *     which case they will be responsible for the reference's deallocation.
 */
typedef struct ColMapping_PyObject {
    PyObject_HEAD
    ColMapping *ref;
} ColMapping_PyObject;

extern PyTypeObject ColMapping_PyType;


// This macro instantiates a new `ColMapping_PyObject` object (with refcnt 1)
#define ColMapping_PyNEW() ((ColMapping_PyObject*) \
    PyObject_CallObject((PyObject*) &ColMapping_PyType, NULL))



ColMapping_PyObject* ColMappingPy_from_array(PyObject *self, PyObject *args);

int init_py_colmapping(PyObject *module);

#endif
