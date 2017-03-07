#include <Python.h>


/**
 * Create and return a new instance of python's None object
 * (actually, this reuses Py_None singleton, increasing its REFCNT)
 */
PyObject* none() {
    Py_RETURN_NONE;
}


/**
 * Make and return a new reference for the provided PyObject
 */
PyObject* makeref(PyObject* x) {
    Py_INCREF(x);
    return x;
}
