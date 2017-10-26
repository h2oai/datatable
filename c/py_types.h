#ifndef dt_PY_TYPES_H
#define dt_PY_TYPES_H
#include "myassert.h"
#include <Python.h>
#include "types.h"
#include "datatable.h"

#if LLONG_MAX==9223372036854775807
    // int64_t == long long int
    dt_static_assert(sizeof(long long) == 8, "Sizeof(long long) != 8");
    #define PyLong_AsInt64             PyLong_AsLongLong
    #define PyLong_AsInt64AndOverflow  PyLong_AsLongLongAndOverflow
    #define PyLong_AsUInt64            PyLong_AsUnsignedLongLong
    #define PyLong_AsUInt64Mask        PyLong_AsUnsignedLongLongMask
    #define PyLong_FromInt64           PyLong_FromLongLong
#elif LONG_MAX==9223372036854775807
    // int64_t == long int
    dt_static_assert(sizeof(long) == 8, "Sizeof(long) != 8");
    #define PyLong_AsInt64             PyLong_AsLong
    #define PyLong_AsInt64AndOverflow  PyLong_AsLongAndOverflow
    #define PyLong_AsUInt64            PyLong_AsUnsignedLong
    #define PyLong_AsUInt64Mask        PyLong_AsUnsignedLongMask
    #define PyLong_FromInt64           PyLong_FromLong
#else
    #error "Bad architecture: int64_t not available..."
#endif

dt_static_assert(sizeof(int64_t) == sizeof(Py_ssize_t),
                 "int64_t and Py_ssize_t should refer to the same type");


typedef PyObject* (stype_formatter)(Column *col, int64_t row);

extern PyObject* py_ltype_names[DT_LTYPES_COUNT];
extern PyObject* py_stype_names[DT_STYPES_COUNT];
extern PyObject* py_ltype_objs[DT_LTYPES_COUNT];
extern PyObject* py_stype_objs[DT_STYPES_COUNT];
extern stype_formatter* py_stype_formatters[DT_STYPES_COUNT];
extern size_t py_buffers_size;


int init_py_types(PyObject *module);
void init_py_stype_objs(PyObject* stype_enum);
void init_py_ltype_objs(PyObject* ltype_enum);

#endif
