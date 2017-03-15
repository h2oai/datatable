#ifndef Dt_DATATABLE_H
#define Dt_DATATABLE_H
#include <Python.h>

typedef enum dt_Coltype {
    DT_AUTO    = 0,  // special "marker" type to indicate that the system should
                     // autodetect the column's type from the data. This value
                     // must not be used in an actual Datatable instance.
    DT_DOUBLE  = 1,
    DT_LONG    = 2,
    DT_STRING  = 3,
    DT_BOOL    = 4,
    DT_OBJECT  = 5
} dt_Coltype;

int dt_Coltype_size[6];


/*--- Main Datatable object --------------------------------------------------*/

typedef struct dt_DatatableObject {
    PyObject_HEAD
    int  ncols;
    long nrows;
    dt_Coltype *coltypes;
    void **columns;
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
