#ifndef Dt_DATATABLE_H
#define Dt_DATATABLE_H
#include <Python.h>

typedef enum dt_Coltype {
    DT_AUTO    = 0,  // special "marker" type to indicate that the system should
                     // autodetect the column's type from the data. This value
                     // cannot be used in an actual Datatable instance.
    DT_DOUBLE  = 1,
    DT_LONG    = 2,
    DT_STRING  = 3,
    DT_BOOL    = 4,
    DT_OBJECT  = 5
} dt_Coltype;

typedef union dt_Coldata {
    double *ddouble;
    long   *dlong;
    char   **dstring;
    unsigned char *dbool;  /* 0 = False, 1 = True, 2 = NaN */
    PyObject **dobject;
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

typedef struct dt_DtViewObject {
    PyObject_HEAD
    int col0;
    int ncols;
    long row0;
    int nrows;
    PyObject *types;
    PyObject *data;
} dt_DtViewObject;

PyTypeObject dt_DtViewType;

#endif
