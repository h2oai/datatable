//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#include <Python.h>
#include "capi.h"
#include "csv/py_csv.h"
#include "csv/writer.h"
#include "expr/py_expr.h"
#include "options.h"
#include "py_column.h"
#include "py_columnset.h"
#include "py_datatable.h"
#include "py_datawindow.h"
#include "py_encodings.h"
#include "py_rowindex.h"
#include "py_types.h"
#include "py_utils.h"


PyMODINIT_FUNC PyInit__datatable(void);
extern PyObject* Py_One, *Py_Zero;

PyObject* Py_One;
PyObject* Py_Zero;



static PyObject* pyexec_function(PyObject *self, PyObject *args)
{
  CATCH_EXCEPTIONS(
    void *fnptr;
    PyObject *fnargs = NULL;

    if (!PyArg_ParseTuple(args, "l|O:exec_function", &fnptr, &fnargs))
        return NULL;

    return ((PyCFunction) fnptr)(self, fnargs);
  );
}


static PyObject* pyregister_function(PyObject*, PyObject *args)
{
  CATCH_EXCEPTIONS(
    int n = -1;
    PyObject *fnref = NULL;

    if (!PyArg_ParseTuple(args, "iO:register_function", &n, &fnref))
        return NULL;
    if (!PyCallable_Check(fnref)) {
        PyErr_SetString(PyExc_TypeError, "parameter `fn` must be callable");
        return NULL;
    }
    Py_XINCREF(fnref);
    if (n == 1) pycolumn::fn_hexview = fnref;
    else if (n == 2) init_py_stype_objs(fnref);
    else if (n == 3) init_py_ltype_objs(fnref);
    else if (n == 4) replace_typeError(fnref);
    else if (n == 5) replace_valueError(fnref);
    else {
        PyErr_Format(PyExc_ValueError, "Incorrect function index: %d", n);
        return NULL;
    }
    return none();
  );
}


static PyObject* pyget_internal_function_ptrs(PyObject*, PyObject*)
{
  #define ADD(f) PyTuple_SetItem(res, i++, PyLong_FromSize_t((size_t) (f)))
  CATCH_EXCEPTIONS(
    const int SIZE = 6;
    int i = 0;
    PyObject *res = PyTuple_New(SIZE);
    if (!res) return NULL;

    ADD(_dt_malloc);
    ADD(_dt_realloc);
    ADD(_dt_free);
    ADD(datatable_get_column_data);
    ADD(datatable_unpack_slicerowindex);
    ADD(datatable_unpack_arrayrowindex);

    assert(i == SIZE);
    return res;
  );
}


static PyObject* pyget_integer_sizes(PyObject*, PyObject*)
{
  CATCH_EXCEPTIONS(
    const int SIZE = 5;
    int i = 0;
    PyObject *res = PyTuple_New(SIZE);
    if (!res) return NULL;

    ADD(sizeof(short int));
    ADD(sizeof(int));
    ADD(sizeof(long int));
    ADD(sizeof(long long int));
    ADD(sizeof(size_t));

    assert(i == SIZE);
    return res;
  );
  #undef ADD
}



//------------------------------------------------------------------------------
// Module definition
//------------------------------------------------------------------------------

#define METHOD0_(name) {#name, (PyCFunction)py ## name, METH_VARARGS, NULL}
#define METHOD1_(name) {#name, (PyCFunction)py ## name, METH_NOARGS, NULL}

static PyMethodDef DatatableModuleMethods[] = {
    METHODv(pycolumnset::columns_from_mixed),
    METHODv(pycolumnset::columns_from_slice),
    METHODv(pycolumnset::columns_from_array),
    METHODv(pycolumnset::columns_from_columns),
    METHODv(pycolumn::column_from_list),
    METHODv(pyrowindex::rowindex_from_slice),
    METHODv(pyrowindex::rowindex_from_slicelist),
    METHODv(pyrowindex::rowindex_from_array),
    METHODv(pyrowindex::rowindex_from_column),
    METHODv(pyrowindex::rowindex_from_filterfn),
    METHODv(pydatatable::datatable_from_list),
    METHODv(pydatatable::datatable_load),
    METHODv(pydatatable::datatable_from_buffers),
    METHODv(pydatatable::install_buffer_hooks),
    METHODv(set_nthreads),
    METHODv(set_core_logger),
    METHODv(gread),
    METHOD0_(write_csv),
    METHOD0_(exec_function),
    METHOD0_(register_function),
    METHOD1_(get_internal_function_ptrs),
    METHOD1_(get_integer_sizes),
    METHODv(expr_binaryop),
    METHODv(expr_cast),
    METHODv(expr_column),
    METHODv(expr_reduceop),
    METHODv(expr_unaryop),

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
    init_exceptions();

    Py_One = PyLong_FromLong(1);
    Py_Zero = PyLong_FromLong(0);

    // Instantiate module object
    PyObject* m = PyModule_Create(&datatablemodule);
    if (m == NULL) return NULL;

    // Initialize submodules
    if (!init_py_types(m)) return NULL;
    if (!pydatawindow::static_init(m)) return NULL;
    if (!pycolumn::static_init(m)) return NULL;
    if (!pycolumnset::static_init(m)) return NULL;
    if (!pydatatable::static_init(m)) return NULL;
    if (!pyrowindex::static_init(m)) return NULL;
    if (!init_py_encodings(m)) return NULL;

    return m;
}
