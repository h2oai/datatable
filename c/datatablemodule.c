#include <Python.h>
#include "datatable.h"



static PyObject* some_function(PyObject* self, PyObject* args) {
    PyObject* result = NULL;
    if (PyArg_ParseTuple(args, "")) {
        result = PyLong_FromLong(0);
    }
    return result;
}



//------------------------------------------------------------------------------
// Module definition
//------------------------------------------------------------------------------

static PyMethodDef DatatableMethods[] = {
    {"test", some_function, METH_VARARGS, "Just a test function"},

    {NULL, NULL, 0, NULL}  /* Sentinel */
};

static struct PyModuleDef datatablemodule = {
    PyModuleDef_HEAD_INIT,
    "_datatable",  /* name of the module */
    "module doc",  /* module documentation */
    -1,            /* size of per-interpreter state of the module, or -1
                      if the module keeps state in global variables */
    DatatableMethods
};

/* Called when Python program imports the module */
PyMODINIT_FUNC
PyInit__datatable(void) {
    PyObject* m;

    dt_DatatableType.tp_new = PyType_GenericNew;
    if (PyType_Ready(&dt_DatatableType) < 0)
        return NULL;

    m = PyModule_Create(&datatablemodule);
    if (m == NULL)
        return NULL;

    Py_INCREF(&dt_DatatableType);
    PyModule_AddObject(m, "DataTable", (PyObject *)&dt_DatatableType);
    return m;
}
