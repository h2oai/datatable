#ifndef dt_PY_TYPES_H
#define dt_PY_TYPES_H
#include <Python.h>
#include "types.h"
#include "datatable.h"

typedef PyObject* (stype_formatter)(Column *col, int64_t row);

extern PyObject* py_ltype_names[DT_LTYPES_COUNT];
extern PyObject* py_stype_names[DT_STYPES_COUNT];
extern stype_formatter* py_stype_formatters[DT_STYPES_COUNT];


int init_py_types(PyObject *module);

#endif
