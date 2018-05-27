//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#include <exception>
#include <stdexcept>
#include <string>
#include <vector>
#include <string.h>  // memcpy
#include "py_datatable.h"
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
    return nullptr;
}


void* _dt_malloc(size_t n)
{
    if (!n) return nullptr;
    void *res = malloc(n);
    if (res == nullptr) {
        PyErr_Format(PyExc_MemoryError, "Failed to allocate %zd bytes", n);
    }
    return res;
}


void* _dt_realloc(void *ptr, size_t n)
{
    if (!n) return nullptr;
    void *res = realloc(ptr, n);
    if (res == nullptr) {
        PyErr_Format(PyExc_MemoryError, "Failed to allocate %zd bytes", n);
    }
    return res;
}


void* _dt_calloc(size_t n, size_t size)
{
    if (!n) return nullptr;
    void *res = calloc(n, size);
    if (res == nullptr) {
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


void log_call(const char* msg) {
  if (!config::logger) return;
  PyObject_CallMethod(config::logger, "info", "s", msg);
}
