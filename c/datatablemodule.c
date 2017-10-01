#include <Python.h>
#include "csv/writer.h"
#include "py_column.h"
#include "py_columnset.h"
#include "py_datatable.h"
#include "py_datawindow.h"
#include "py_encodings.h"
#include "py_fread.h"
#include "py_rowindex.h"
#include "py_types.h"
#include "py_utils.h"


PyMODINIT_FUNC PyInit__datatable(void);
PyObject* pyfn_column_hexview = NULL;  // Defined in py_column.h


static PyObject* pyexec_function(PyObject *self, PyObject *args)
{
    void *fnptr;
    PyObject *fnargs = NULL;

    if (!PyArg_ParseTuple(args, "l|O:exec_function", &fnptr, &fnargs))
        return NULL;

    return ((PyCFunction) fnptr)(self, fnargs);
}


static PyObject* pyregister_function(UU, PyObject *args)
{
    int n = -1;
    PyObject *fnref = NULL;

    if (!PyArg_ParseTuple(args, "iO:register_function", &n, &fnref))
        return NULL;
    if (!PyCallable_Check(fnref)) {
        PyErr_SetString(PyExc_TypeError, "parameter `fn` must be callable");
        return NULL;
    }
    Py_XINCREF(fnref);
    if (n == 1) pyfn_column_hexview = fnref;
    else {
        PyErr_Format(PyExc_ValueError, "Incorrect function index: %d", n);
        return NULL;
    }
    return none();
}


static PyObject* pyget_internal_function_ptrs(UU, UU1)
{
    int i = 0;
    PyObject *res = PyTuple_New(5);
    if (!res) return NULL;

    #define ADD(f) PyTuple_SetItem(res, i++, PyLong_FromSize_t((size_t)f))
    ADD(_dt_malloc);
    ADD(_dt_realloc);
    ADD(_dt_free);
    ADD(make_data_column);
    ADD(rowindex_from_filterfn32);
    #undef ADD

    return res;
}



//------------------------------------------------------------------------------
// Module definition
//------------------------------------------------------------------------------

#define METHOD0(name) {#name, (PyCFunction)py ## name, METH_VARARGS, NULL}
#define METHOD1(name) {#name, (PyCFunction)py ## name, METH_NOARGS, NULL}

static PyMethodDef DatatableModuleMethods[] = {
    METHOD0(columns_from_mixed),
    METHOD0(columns_from_slice),
    METHOD0(columns_from_array),
    METHOD0(rowindex_from_slice),
    METHOD0(rowindex_from_slicelist),
    METHOD0(rowindex_from_array),
    METHOD0(rowindex_from_boolcolumn),
    METHOD0(rowindex_from_intcolumn),
    METHOD0(rowindex_from_filterfn),
    METHOD0(rowindex_from_function),
    METHOD0(rowindex_uplift),
    METHOD0(datatable_assemble),
    METHOD0(datatable_from_list),
    METHOD0(datatable_load),
    METHOD0(datatable_from_buffers),
    METHOD0(fread),
    METHOD0(write_csv),
    METHOD0(exec_function),
    METHOD0(register_function),
    METHOD0(install_buffer_hooks),
    METHOD1(get_internal_function_ptrs),

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
    init_csvwrite_constants();

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
