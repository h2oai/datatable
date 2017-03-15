#include <Python.h>
#include "datatable.h"
#include "datawindow.h"
#include "dtutils.h"
#include "rows.h"



//------------------------------------------------------------------------------
// Module definition
//------------------------------------------------------------------------------

static PyMethodDef DatatableModuleMethods[] = {
    {"rows_from_slice", rows_from_slice, METH_VARARGS,
        "Row selector constructed from a slice of rows"},
    {"rows_from_array", rows_from_array, METH_VARARGS,
        "Row selector constructed from a list of row indices"},

    {NULL, NULL, 0, NULL}  /* Sentinel */
};

static PyModuleDef datatablemodule = {
    PyModuleDef_HEAD_INIT,
    "_datatable",  /* name of the module */
    "module doc",  /* module documentation */
    -1,            /* size of per-interpreter state of the module, or -1
                      if the module keeps state in global variables */
    DatatableModuleMethods
};

/* Called when Python program imports the module */
PyMODINIT_FUNC
PyInit__datatable(void) {
    PyObject *m;

    // Sanity checks
    assert(sizeof(char) == sizeof(unsigned char));

    dt_Coltype_size[DT_AUTO] = 0;
    dt_Coltype_size[DT_DOUBLE] = sizeof(double);
    dt_Coltype_size[DT_LONG] = sizeof(long);
    dt_Coltype_size[DT_BOOL] = sizeof(char);
    dt_Coltype_size[DT_STRING] = sizeof(char*);
    dt_Coltype_size[DT_OBJECT] = sizeof(PyObject*);

    Py_int0 = PyLong_FromLong(0);
    Py_int1 = PyLong_FromLong(1);

    dt_DatatableType.tp_new = PyType_GenericNew;
    dt_DataWindowType.tp_new = PyType_GenericNew;
    dt_RowsIndexType.tp_new = PyType_GenericNew;
    if (PyType_Ready(&dt_DatatableType) < 0 ||
        PyType_Ready(&dt_DataWindowType) < 0 ||
        PyType_Ready(&dt_RowsIndexType) < 0)
        return NULL;

    m = PyModule_Create(&datatablemodule);
    if (m == NULL)
        return NULL;

    Py_INCREF(&dt_DatatableType);
    Py_INCREF(&dt_DataWindowType);
    Py_INCREF(&dt_RowsIndexType);
    PyModule_AddObject(m, "DataTable", (PyObject*) &dt_DatatableType);
    PyModule_AddObject(m, "DataWindow", (PyObject*) &dt_DataWindowType);
    PyModule_AddObject(m, "RowsIndex", (PyObject*) &dt_RowsIndexType);
    return m;
}
