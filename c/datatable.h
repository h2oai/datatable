#include <Python.h>

typedef enum dt_Coltype {
    DT_DOUBLE,
    DT_LONG,
    DT_STRING,
    DT_BOOL,
    DT_OBJECT
} dt_Coltype;

typedef union dt_Coldata {
    double *ddouble;
    long   *dlong;
    char*  *dstring;
    unsigned char *dbool;  /* 0 = False, 1 = True, 2 = NaN */
    PyObject* *dobject;
} dt_Coldata;



/*--- Main Datatable object --------------------------------------------------*/

typedef struct dt_DatatableObject {
    PyObject_HEAD
    int  ncols;
    long nrows;
    dt_Coltype *coltypes;
    dt_Coldata *columns;
} dt_DatatableObject;

PyTypeObject dt_DatatableType;



/*--- Message type for transferring Datatable's data into Python -------------*/

typedef struct dt_DtWindowObject {
    PyObject_HEAD
    int col0;
    int ncols;
    long row0;
    int nrows;
    PyObject* data;
} dt_DtWindowObject;

PyTypeObject dt_DtWindowType;
