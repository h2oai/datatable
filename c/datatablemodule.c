#include <Python.h>
#include "py_colmapping.h"
#include "py_datatable.h"
#include "py_datawindow.h"
#include "py_rowmapping.h"
#include "fread_impl.h"
#include "dtutils.h"

PyMODINIT_FUNC PyInit__datatable(void);



//------------------------------------------------------------------------------
// Module definition
//------------------------------------------------------------------------------

static PyMethodDef DatatableModuleMethods[] = {
    {"rowmapping_from_slice", (PyCFunction)RowMappingPy_from_slice,
        METH_VARARGS,
        "Row selector constructed from a slice of rows"},
    {"rowmapping_from_slicelist", (PyCFunction)RowMappingPy_from_slicelist,
        METH_VARARGS,
        "Row selector constructed from a 'slicelist'"},
    {"rowmapping_from_array", (PyCFunction)RowMappingPy_from_array,
        METH_VARARGS,
        "Row selector constructed from a list of row indices"},
    {"rowmapping_from_filter", (PyCFunction)RowMappingPy_from_filter,
        METH_VARARGS,
        "Row selector constructed using a filter function"},
    {"rowmapping_from_column", (PyCFunction)RowMappingPy_from_column,
        METH_VARARGS,
        "Row selector constructed from a single-boolean-column datatable"},
    {"colmapping_from_array", (PyCFunction)ColMappingPy_from_array,
        METH_VARARGS,
        "Column selector constructed from a list of column indices"},
    {"datatable_from_list", (PyCFunction)dt_DataTable_fromlist, METH_VARARGS,
        "Create Datatable from a list"},
    {"fread", (PyCFunction)freadPy, METH_VARARGS,
        "Read a text file and convert into a datatable"},

    {NULL, NULL, 0, NULL}  /* Sentinel */
};

static PyModuleDef datatablemodule = {
    PyModuleDef_HEAD_INIT,
    "_datatable",  /* name of the module */
    "module doc",  /* module documentation */
    -1,            /* size of per-interpreter state of the module, or -1
                      if the module keeps state in global variables */
    DatatableModuleMethods,
    0,0,0,0,
};

/* Called when Python program imports the module */
PyMODINIT_FUNC
PyInit__datatable(void) {
    // Sanity checks
    assert(sizeof(char) == sizeof(unsigned char));
    assert(sizeof(void) == 1);
    assert('\0' == (char)0);
    assert(sizeof(size_t) == sizeof(int64_t));  // should this be an assert?

    // Instantiate module object
    PyObject *m = PyModule_Create(&datatablemodule);
    if (m == NULL) return NULL;

    // Initialize submodules
    if (!init_py_datatable(m)) return NULL;
    if (!init_py_datawindow(m)) return NULL;
    if (!init_py_rowmapping(m)) return NULL;
    if (!init_py_colmapping(m)) return NULL;

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
