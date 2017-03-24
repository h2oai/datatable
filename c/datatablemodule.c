#include <Python.h>
#include "py_datatable.h"
#include "py_datawindow.h"
#include "py_rowmapping.h"
#include "dtutils.h"



//------------------------------------------------------------------------------
// Module definition
//------------------------------------------------------------------------------

static PyMethodDef DatatableModuleMethods[] = {
    {"rowmapping_from_slice", (PyCFunction)RowMappingPy_from_slice,
        METH_VARARGS,
        "Row selector constructed from a slice of rows"},
    {"rowmapping_from_array", (PyCFunction)RowMappingPy_from_array,
        METH_VARARGS,
        "Row selector constructed from a list of row indices"},
    {"rowmapping_from_filter", (PyCFunction)RowMappingPy_from_filter,
        METH_VARARGS,
        "Row selector constructed using a filter function"},
    {"rowmapping_from_column", (PyCFunction)RowMappingPy_from_column,
        METH_VARARGS,
        "Row selector constructed from a single-boolean-column datatable"},
    {"datatable_from_list", (PyCFunction)dt_DataTable_fromlist, METH_VARARGS,
        "Create Datatable from a list"},

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
    // Sanity checks
    assert(sizeof(char) == sizeof(unsigned char));

    // Instantiate module object
    PyObject *m = PyModule_Create(&datatablemodule);
    if (m == NULL) return NULL;

    // Initialize submodules
    if (!init_py_datatable(m)) return NULL;
    if (!init_py_datawindow(m)) return NULL;
    if (!init_py_rowmapping(m)) return NULL;

    ColType_size[DT_AUTO] = 0;
    ColType_size[DT_DOUBLE] = sizeof(double);
    ColType_size[DT_LONG] = sizeof(long);
    ColType_size[DT_BOOL] = sizeof(char);
    ColType_size[DT_STRING] = sizeof(char*);
    ColType_size[DT_OBJECT] = sizeof(PyObject*);

    Py_int0 = PyLong_FromLong(0);
    Py_int1 = PyLong_FromLong(1);

    return m;
}
