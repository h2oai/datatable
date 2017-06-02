/*******************************************************************************
 * Collection of macros / helper functions to facilitate translation from
 * Python to C.
 ******************************************************************************/
#ifndef dt_PYUTILS_H
#define dt_PYUTILS_H
#include <Python.h>
#include "utils.h"


/**
 * This macro can be inserted into a function's signature in place of an unused
 * `PyObject*` parameter, for example:
 *     PyObject* make_schmoo(UU, PyObject *args) { ... }
 * Use macros UU1 and UU2 if you need more than one unused PyObject*. If you
 * need to have an unused parameter of any other type, use macro `UNUSED`
 * directly (the macro is defined in `utils.h`).
 */
#define UU   UNUSED(PyObject *_unused0)
#define UU1  UNUSED(PyObject *_unused1)
#define UU2  UNUSED(PyObject *_unused2)



PyObject* none(void);
PyObject* incref(PyObject *x);
PyObject* decref(PyObject *x);
void* clone(void *src, size_t n_bytes);



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
        dtmalloc(res, char, len + 1);                                          \
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
