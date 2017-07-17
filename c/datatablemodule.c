#include <Python.h>
#include "py_column.h"
#include "py_columnset.h"
#include "py_datatable.h"
#include "py_datawindow.h"
#include "py_encodings.h"
#include "py_fread.h"
#include "py_rowindex.h"
#include "py_types.h"
#include "py_utils.h"

#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>   // for open()

PyMODINIT_FUNC PyInit__datatable(void);

// where should this be moved?
PyObject* exec_function(PyObject *self, PyObject *args);

PyObject* exec_function(PyObject *self, PyObject *args)
{
    void *fnptr;
    PyObject *fnargs = NULL;

    if (!PyArg_ParseTuple(args, "l|O:exec_function", &fnptr, &fnargs))
        return NULL;

    return ((PyCFunction) fnptr)(self, fnargs);
}


//------------------------------------------------------------------------------
// Module definition
//------------------------------------------------------------------------------

#define METHOD0(name) {#name, (PyCFunction)py ## name, METH_VARARGS, NULL}

static PyMethodDef DatatableModuleMethods[] = {
    METHOD0(columns_from_pymixed),
    METHOD0(columns_from_slice),
    METHOD0(columns_from_array),
    METHOD0(datatable_assemble),
    METHOD0(rowindex_from_slice),
    METHOD0(rowindex_from_slicelist),
    METHOD0(rowindex_from_array),
    METHOD0(rowindex_from_column),
    METHOD0(rowindex_from_filterfn),
    METHOD0(datatable_from_list),
    METHOD0(datatable_load),
    {"fread", (PyCFunction)freadPy, METH_VARARGS,
        "Read a text file and convert into a datatable"},
    {"exec_function", (PyCFunction)exec_function, METH_VARARGS,
        "Execute a PyCFunction passed as a pointer"},

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

    // Instantiate module object
    PyObject *m = PyModule_Create(&datatablemodule);
    if (m == NULL) return NULL;

    // Initialize submodules
    if (!init_py_datatable(m)) return NULL;
    if (!init_py_datawindow(m)) return NULL;
    if (!init_py_rowindex(m)) return NULL;
    if (!init_py_types(m)) return NULL;
    if (!init_py_column(m)) return NULL;
    if (!init_py_columnset(m)) return NULL;
    if (!init_py_encodings(m)) return NULL;

    return m;
}
