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

#define DT_METHOD1es(name) \
    {dtvar_##name, (PyCFunction)pyes_##name, METH_VARARGS, dtdoc_##name}


PyObject* none(void);
PyObject* incref(PyObject *x);
PyObject* decref(PyObject *x);

#define pyfree(ptr)  do { Py_XDECREF(ptr); ptr = NULL; } while(0)


#endif
