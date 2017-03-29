#include "dtutils.h"


/**
 * Create and return a new instance of python's None object
 * (actually, this reuses Py_None singleton, increasing its REFCNT)
 */
PyObject* none(void) {
    Py_RETURN_NONE;
}


/**
 * Make and return a new reference for the provided PyObject
 */
PyObject* incref(PyObject *x) {
    Py_INCREF(x);
    return x;
}


/**
 * "Unmake" the provided object and return NULL
 */
PyObject* decref(PyObject *x) {
    Py_XDECREF(x);
    return NULL;
}


static void set_oom_error_message(long n) {
    static char str[80];
    sprintf(str, "Out of memory: unable to allocate %ld bytes", n);
    PyErr_SetString(PyExc_RuntimeError, str);
}


void* clone(void *src, long n_bytes) {
    void* copy = malloc((size_t)n_bytes);
    if (copy == NULL) {
        set_oom_error_message(n_bytes);
        return NULL;
    }
    if (src != NULL) {
        memcpy(copy, src, n_bytes);
    }
    return copy;
}

PyObject *Py_int0;
PyObject *Py_int1;
