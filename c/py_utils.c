#include <string.h>  // memcpy
#include "py_utils.h"


/**
 * Return clone of the memory buffer `src` of size `n_bytes`. This function
 * will allocate and return new memory buffer for the copy, and it will be the
 * responsibility of the caller to free the cloned buffer.
 * If this function is unable to allocate the necessary memory range, it will
 * set an error message, and return NULL.
 */
void* clone(void *src, size_t n_bytes) {
    void* copy = malloc(n_bytes);
    if (copy == NULL) {
        PyErr_Format(PyExc_RuntimeError,
            "Out of memory: unable to allocate %ld bytes", n_bytes);
        return NULL;
    }
    if (src != NULL) {
        memcpy(copy, src, n_bytes);
    }
    return copy;
}


/**
 * Create and return a new instance of python's None object
 * (actually, this reuses Py_None singleton, increasing its REFCNT)
 */
PyObject* none(void) {
    Py_INCREF(Py_None);
    return Py_None;
}


/**
 * Increment reference count of PyObject `x`, and then return it.
 */
PyObject* incref(PyObject *x) {
    Py_XINCREF(x);
    return x;
}


/**
 * Decrement reference count of PyObject `x`, and then return NULL.
 */
PyObject* decref(PyObject *x) {
    Py_XDECREF(x);
    return NULL;
}


void* _dt_malloc(size_t n)
{
    void *res = malloc(n);
    if (res == NULL) {
        PyErr_Format(PyExc_MemoryError, "Failed to allocate %zd bytes", n);
    }
    return res;
}


void* _dt_realloc(void *ptr, size_t n)
{
    void *res = realloc(ptr, n);
    if (res == NULL) {
        PyErr_Format(PyExc_MemoryError, "Failed to allocate %zd bytes", n);
    }
    return res;
}


void* _dt_calloc(size_t n, size_t size)
{
    void *res = calloc(n, size);
    if (res == NULL) {
        PyErr_Format(PyExc_MemoryError, "Failed to allocate %zd bytes",
                     n * size);
    }
    return res;
}


void _dt_free(void *ptr)
{
    free(ptr);
}


char* _to_string(PyObject *x, _Bool copy)
{
    if (x == NULL) return (char*)-1;
    char *res = NULL;
    if (x != Py_None) {
        char *buf = NULL;
        size_t len = 0;
        if (PyUnicode_Check(x)) {
            PyObject *y = PyUnicode_AsEncodedString(x, "utf-8", "strict");
            if (y == NULL) return (char*)-1;
            buf = PyBytes_AsString(y);
            len = (size_t) PyBytes_Size(y);
            dtmalloc(res, char, len + 1);
            memcpy(res, buf, len + 1);
            copy = 0;
            Py_DECREF(y);
        }
        if (PyBytes_Check(x)) {
            buf = PyBytes_AsString(x);
            len = (size_t) PyBytes_Size(x);
        }
        if (copy) {
            dtmalloc(res, char, len + 1);
            memcpy(res, buf, len + 1);
        } else {
            res = buf;
        }
    }
    Py_DECREF(x);
    return res;
}
