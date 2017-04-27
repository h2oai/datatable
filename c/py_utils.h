/*******************************************************************************
 * Collection of macros / helper functions to facilitate translation from
 * Python to C.
 ******************************************************************************/
#ifndef dt_PYUTILS_H
#define dt_PYUTILS_H
#include <Python.h>
#include "utils.h"



/**
 * Create and return a new instance of python's None object
 * (actually, this reuses Py_None singleton, increasing its REFCNT)
 */
#define none() incref(Py_None)


/**
 * Return PyObject* `x`, after having its reference count increased.
 */
#define incref(x) ({ PyObject *y = x; Py_XINCREF(y); y; })


/**
 * Decrement reference count of `x` and return NULL.
 */
#define decref(x) ({ Py_XDECREF(x); NULL; })


/**
 * Return clone of the memory buffer `src` of size `n_bytes`. This function
 * will allocate and return new memory buffer for the copy, and it will be the
 * responsibility of the caller to free the cloned buffer.
 * If this function is unable to allocate the necessary memory range, it will
 * set an error message, and return NULL.
 */
void* clone(void *src, size_t n_bytes);



/**
 * Similar to `malloc(n)`, but in case of error set the error string and then
 * execute "goto fail".
 */
#define MALLOC(n) ({                                                           \
    void *malloc_res = malloc((size_t)(n));                                    \
    if (malloc_res == NULL) {                                                  \
        PyErr_Format(PyExc_MemoryError, "Failed to allocate %zd bytes", n);    \
        goto fail;                                                             \
    }                                                                          \
    malloc_res;                                                                \
})


/**
 * Similar to `calloc(s, n)`, but in case of error set the error string and
 * then execute "goto fail".
 */
#define CALLOC(s, n) ({                                                        \
    void *calloc_res = calloc(s, (size_t)(n));                                 \
    if (calloc_res == NULL) {                                                  \
        size_t sz = (s)*(size_t)(n);                                           \
        PyErr_Format(PyExc_MemoryError, "Failed to allocate %zd bytes", sz);   \
        goto fail;                                                             \
    }                                                                          \
    calloc_res;                                                                \
})


/**
 * Similar to `realloc(n)`, but in case of error set the error string and then
 * execute "goto fail".
 */
#define REALLOC(ptr, n) ({                                                     \
    void *realloc_res = realloc(ptr, (size_t)(n));                             \
    if (realloc_res == NULL) {                                                 \
        PyErr_Format(PyExc_MemoryError, "Failed to allocate %zd bytes", n);    \
        goto fail;                                                             \
    }                                                                          \
    realloc_res;                                                               \
})


/**
 * Returned named attribute of a python object. This is a shortcut for
 * ``PyObject_GetAttrString``.
 */
#define ATTR(pyobj, attr)  PyObject_GetAttrString(pyobj, attr)


/**
 * Convert the provided PyObject* into a C UTF-8-encoded string; this macro
 * will "goto fail" if there are any errors.
 * The returned result is a newly allocated memory buffer, and it is the
 * responsibility of the caller to free it.
 */
#define TOSTRING(pyobj, dflt) ({                                               \
    char *res = dflt;                                                          \
    PyObject *x = pyobj;                                                       \
    if (x == NULL) goto fail;                                                  \
    if (x != Py_None) {                                                        \
        PyObject *y = PyUnicode_AsEncodedString(x, "utf-8", "strict");         \
        if (y == NULL) goto fail;                                              \
        char *buf = PyBytes_AsString(y);                                       \
        Py_DECREF(y);                                                          \
        size_t len = (size_t) PyBytes_Size(y);                                 \
        res = MALLOC(len + 1);                                                 \
        memcpy(res, buf, len + 1);                                             \
    }                                                                          \
    Py_DECREF(x);                                                              \
    res;                                                                       \
})


#define TOCHAR(pyobj, dflt) ({                                                 \
    char res = dflt;                                                           \
    PyObject *x = pyobj;                                                       \
    if (x == NULL) goto fail;                                                  \
    if (x != Py_None) {                                                        \
        res = (char)PyUnicode_ReadChar(x, 0);                                  \
    }                                                                          \
    Py_DECREF(x);                                                              \
    res;                                                                       \
})


#define TOINT64(pyobj, dflt) ({                                                \
    int64_t res = dflt;                                                        \
    PyObject *x = pyobj;                                                       \
    if (x == NULL) goto fail;                                                  \
    if (x != Py_None) {                                                        \
        res = PyLong_AsLongLong(x);                                            \
    }                                                                          \
    Py_DECREF(x);                                                              \
    res;                                                                       \
})


#define TOBOOL(pyobj, dflt) ({                                                 \
    int8_t res = dflt;                                                         \
    PyObject *x = pyobj;                                                       \
    if (x == NULL) goto fail;                                                  \
    if (x != Py_None) {                                                        \
        res = (x == Py_True);                                                  \
    }                                                                          \
    Py_DECREF(x);                                                              \
    res;                                                                       \
})


#define TOSTRINGLIST(pyobj, dflt) ({                                           \
    char **res = dflt;                                                         \
    PyObject *x = pyobj;                                                       \
    if (x == NULL) goto fail;                                                  \
    if (x != Py_None) {                                                        \
        Py_ssize_t count = PyList_Size(x);                                     \
        res = calloc(sizeof(char*), (size_t)(count + 1));                      \
        for (Py_ssize_t i = 0; i < count; i++) {                               \
            PyObject *item = PyList_GetItem(x, i);                             \
            PyObject *y = PyUnicode_AsEncodedString(item, "utf-8", "strict");  \
            if (y == NULL) {                                                   \
                for (int j = 0; j < i; j++) free(res[j]);                      \
                free(res);                                                     \
                goto fail;                                                     \
            }                                                                  \
            res[i] = PyBytes_AsString(y);                                      \
            Py_DECREF(y);                                                      \
        }                                                                      \
    }                                                                          \
    Py_DECREF(x);                                                              \
    res;                                                                       \
})


#endif
