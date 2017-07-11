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


char* _to_string(PyObject *x, PyObject **tmp)
{
    if (x == NULL) goto fail;
    if (x == Py_None) {
        Py_DECREF(x);
        return NULL;
    } else {
        PyObject *z = NULL;
        if (PyUnicode_Check(x)) {
            z = PyUnicode_AsEncodedString(x, "utf-8", "strict");
            Py_DECREF(x);
        } else if (PyBytes_Check(x)) {
            z = x;
        }
        if (z == NULL) goto fail;
        char *out = NULL;
        if (tmp == NULL) {
            dtmalloc_g(out, char, PyBytes_Size(z) + 1);
            memcpy(out, PyBytes_AsString(z), PyBytes_Size(z) + 1);
        } else {
            *tmp = z;
            out = PyBytes_AsString(z);
        }
        return out;
    }
  fail:
    return (char*) -1;
}

char** _to_string_list(PyObject *x) {
    if (x == NULL) goto fail;
    char **res = NULL;
    if (x == Py_None) {}
    else if (PyList_Check(x)) {
        Py_ssize_t count = PyList_Size(x);
        dtcalloc(res, char*, count + 1);
        for (Py_ssize_t i = 0; i < count; i++) {
            PyObject *item = PyList_GetItem(x, i);
            if (PyUnicode_Check(item)) {
                PyObject *y = PyUnicode_AsEncodedString(item, "utf-8", "strict");
                if (y == NULL) goto fail;
                size_t len = (size_t) PyBytes_Size(y);
                dtmalloc_g(res[i], char, len + 1);
                memcpy(res[i], PyBytes_AsString(y), len + 1);
                Py_DECREF(y);
            } else
            if (PyBytes_Check(item)) {
                size_t len = (size_t) PyBytes_Size(x);
                dtmalloc_g(res[i], char, len + 1);
                memcpy(res[i], PyBytes_AsString(x), len + 1);
            } else {
                PyErr_Format(PyExc_TypeError,
                    "Argument %d in the list is not a string: %R (%R)",
                    i, item, PyObject_Type(item));
                goto fail;
            }
        }
    } else {
        PyErr_Format(PyExc_TypeError,
            "A list of strings is expected, got %R", x);
        goto fail;
    }
    Py_DECREF(x);
    return res;
  fail:
    return (char**) -1;
}
