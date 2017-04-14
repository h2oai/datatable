#ifndef dt_PY_EVALUATOR_H
#define dt_PY_EVALUATOR_H
#include <Python.h>
#include "datatable.h"
#include "rowmapping.h"

typedef union Value {
  int64_t i8;
  int32_t i4;
  int16_t i2;
  int8_t  i1;
  double  f8;
  float   f4;
  Column *col;
  DataTable *dt;
  RowMapping *rowmap;
  void *ptr;
} Value;


typedef struct Evaluator_PyObject {
    PyObject_HEAD
    PyObject *pystack;
    Value *stack;
} Evaluator_PyObject;

extern PyTypeObject Evaluator_PyType;


#define Evaluator_PyNEW() ((Evaluator_PyObject*) \
    PyObject_CallObject((PyObject*) &Evaluator_PyType, NULL))



int init_py_evaluator(PyObject *module);

#endif
