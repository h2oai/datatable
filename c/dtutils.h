#ifndef Dt_DTUTILS_H
#define Dt_DTUTILS_H
#include <Python.h>

PyObject* none(void);

PyObject* incref(PyObject* x);

PyObject* decref(PyObject* x);

void* clone(void *src, long n_bytes);

PyObject *Py_int0;
PyObject *Py_int1;


#endif
