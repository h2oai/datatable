#include <Python.h>


typedef struct {
    PyObject_HEAD
    union {
        double* ddouble;
        long* dlong;
        unsigned char* dbool;
        char** dstring;
    } data;
} dt_ColumnObject;

typedef struct {
    PyObject_HEAD
    int numcols;
    long numrows;
    dt_ColumnObject columns[];
} dt_DatatableObject;


PyTypeObject dt_DatatableType;
