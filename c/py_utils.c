#include <string.h>  // memcpy
#include "py_utils.h"


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
    if (!n) return NULL;
    void *res = malloc(n);
    if (res == NULL) {
        PyErr_Format(PyExc_MemoryError, "Failed to allocate %zd bytes", n);
    }
    return res;
}


void* _dt_realloc(void *ptr, size_t n)
{
    if (!n) return NULL;
    void *res = realloc(ptr, n);
    if (res == NULL) {
        PyErr_Format(PyExc_MemoryError, "Failed to allocate %zd bytes", n);
    }
    return res;
}


void* _dt_calloc(size_t n, size_t size)
{
    if (!n) return NULL;
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


__attribute__ ((format(printf, 1, 2)))
void _dt_err_r(const char *format, ...)
{
    va_list vargs;
    va_start(vargs, format);
    PyErr_FormatV(PyExc_RuntimeError, format, vargs);
    va_end(vargs);
}


__attribute__ ((format(printf, 1, 2)))
void _dt_err_v(const char *format, ...)
{
    va_list vargs;
    va_start(vargs, format);
    PyErr_FormatV(PyExc_ValueError, format, vargs);
    va_end(vargs);
}


__attribute__ ((format(printf, 1, 2)))
void _dt_err_a(const char *format, ...)
{
    va_list vargs;
    va_start(vargs, format);
    PyErr_FormatV(PyExc_AssertionError, format, vargs);
    va_end(vargs);
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
            memcpy(out, PyBytes_AsString(z), (size_t)PyBytes_Size(z) + 1);
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
    if (x == NULL) return (char**) -1;
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
