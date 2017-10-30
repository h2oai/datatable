/*******************************************************************************
 * Collection of macros / helper functions to facilitate translation from
 * Python to C.
 ******************************************************************************/
#ifndef dt_PYUTILS_H
#define dt_PYUTILS_H
#include <Python.h>
#include "utils.h"

#define DECLARE_API_FUNCTION(name, docstring, home)                            \
  namespace py {                                                               \
    PyObject* name(PyObject* self, PyObject* args);                            \
    IF1(home                                                                   \
    , char doc_##name[] = docstring;                                           \
      char name_##name[] = #name;                                              \
    , extern char doc_##name[];                                                \
      extern char name_##name[];                                               \
    )                                                                          \
  }


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
