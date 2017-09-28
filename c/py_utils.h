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


#define DT_DOCS(name, doc) \
    static char dtvar_##name[] = #name; \
    static char dtdoc_##name[] = doc;

#define DT_GETSETTER(name) \
    {dtvar_##name, (getter)get_##name, NULL, dtdoc_##name, NULL}

#define DT_METHOD1(name) \
    {dtvar_##name, (PyCFunction)meth_##name, METH_VARARGS, dtdoc_##name}


PyObject* none(void);
PyObject* incref(PyObject *x);
PyObject* decref(PyObject *x);

#define pyfree(ptr)  do { Py_XDECREF(ptr); ptr = NULL; } while(0)


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
#define TOSTRING(pyobj, tmp) ({                                                \
    char *res = _to_string(pyobj, tmp);                                        \
    if ((int64_t)res == -1) goto fail;                                         \
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


#define TOSTRINGLIST(pyobj) ({                                                 \
    char **res = _to_string_list(pyobj);                                       \
    if ((int64_t)res == -1) goto fail;                                         \
    res;                                                                       \
})


bool    get_attr_bool(PyObject *pyobj, const char *attr, bool dflt=false);
int64_t get_attr_int64(PyObject *pyobj, const char *attr, int64_t dflt=0);
char**  get_attr_stringlist(PyObject *pyobj, const char *attr);

char* _to_string(PyObject *x, PyObject **tmp);
char** _to_string_list(PyObject *x);

#endif
