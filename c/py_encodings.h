#ifndef dt_PY_ENCODINGS_H
#define dt_PY_ENCODINGS_H
#include <Python.h>
#include "encodings.h"


int init_py_encodings(PyObject *module);

int decode_windows1252(const unsigned char *__restrict__ src, int len,
                       unsigned char *__restrict__ dest);
int decode_windows1251(const unsigned char *__restrict__ src, int len,
                       unsigned char *__restrict__ dest);


#endif
