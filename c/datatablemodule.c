#include <Python.h>
#include "py_colmapping.h"
#include "py_datatable.h"
#include "py_datawindow.h"
#include "py_evaluator.h"
#include "py_rowmapping.h"
#include "py_types.h"
#include "fread_impl.h"
#include "dtutils.h"

#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>   // for open()

PyMODINIT_FUNC PyInit__datatable(void);

// where should this be moved?
PyObject *dt_from_memmap(PyObject *self, PyObject *args);

PyObject *dt_from_memmap(PyObject *self, PyObject *args)
{
    PyObject *list;

    if (!PyArg_ParseTuple(args, "O!", &PyList_Type, &list))
        return NULL;

    int64_t ncols = PyList_Size(list);

    DataTable *dt = malloc(sizeof(DataTable));
    if (dt == NULL) return NULL;
    dt->ncols = ncols;
    dt->source = NULL;
    dt->rowmapping = NULL;
    dt->columns = malloc(sizeof(Column) * (size_t)ncols);
    if (dt->columns == NULL) return NULL;

    int64_t nrows = -1;
    for (int i = 0; i < ncols; i++) {
        PyObject *item = PyList_GetItem(list, i);
        char *colname = PyUnicode_AsUTF8(item);
        char *dotptr = strrchr(colname, '.');
        if (dotptr == NULL) {
            PyErr_Format(PyExc_ValueError, "Cannot find . in column %s", colname);
            return NULL;
        }
        DataSType elemtype = strcmp(dotptr + 1, "bool") == 0? DT_BOOLEAN_I8 :
                             strcmp(dotptr + 1, "int64") == 0? DT_INTEGER_I64 :
                             strcmp(dotptr + 1, "double") == 0? DT_REAL_F64 : 0;
        if (!elemtype) {
            PyErr_Format(PyExc_ValueError, "Unknown column type: %s", dotptr + 1);
            return NULL;
        }
        size_t elemsize = stype_info[elemtype].elemsize;

        // Memory-map the file
        int fd = open(colname, O_RDONLY);
        if (fd == -1) {
            PyErr_Format(PyExc_RuntimeError, "Error opening file %s: %d", colname, errno);
            return NULL;
        }
        struct stat stat_buf;
        if (fstat(fd, &stat_buf) == -1) {
            close(fd);
            PyErr_Format(PyExc_RuntimeError, "File opened but couldn't obtain its size: %s", colname);
            return NULL;
        }
        int64_t filesize = stat_buf.st_size;
        if (filesize <= 0) {
            close(fd);
            PyErr_Format(PyExc_RuntimeError, "File is empty: %s", colname);
            return NULL;
        }
        void *mmp = mmap(NULL, (size_t)filesize, PROT_READ, MAP_PRIVATE, fd, 0);
        if (mmp == MAP_FAILED) {
            close(fd);
            PyErr_Format(PyExc_RuntimeError, "Failed to memory-map the file: %s", colname);
            return NULL;
        }
        // This is no longer needed
        close(fd);

        int64_t nelems = filesize / (int64_t)elemsize;
        if (nrows == -1)
            nrows = nelems;
        else if (nrows != nelems) {
            PyErr_Format(PyExc_RuntimeError, "File %s contains %lld rows, whereas previous column(s) had %lld rows",
                         colname, nelems, nrows);
            return NULL;
        }

        dt->columns[i].stype = elemtype;
        dt->columns[i].srcindex = -1;
        dt->columns[i].data = mmp;
        dt->columns[i].mmapped = 1;
    }

    dt->nrows = nrows;

    DataTable_PyObject *pydt = DataTable_PyNew();
    if (pydt == NULL) return NULL;
    pydt->ref = dt;
    pydt->source = NULL;

    return (PyObject*)pydt;
}


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
    {"dt_from_memmap", (PyCFunction)dt_from_memmap, METH_VARARGS,
        "Load DataTable from the pdt files"},

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
    // Used in llvm.py
    assert(sizeof(long long int) == sizeof(int64_t));
    assert(sizeof(DataSType) == sizeof(int));
    // If this is not true, then we won't be able to memory-map files, create
    // arrays with more than 2**31 elements, etc.
    assert(sizeof(size_t) >= sizeof(int64_t));

    // Instantiate module object
    PyObject *m = PyModule_Create(&datatablemodule);
    if (m == NULL) return NULL;

    // Initialize submodules
    if (!init_py_datatable(m)) return NULL;
    if (!init_py_datawindow(m)) return NULL;
    if (!init_py_rowmapping(m)) return NULL;
    if (!init_py_colmapping(m)) return NULL;
    if (!init_py_evaluator(m)) return NULL;
    if (!init_py_types(m)) return NULL;

    Py_int0 = PyLong_FromLong(0);
    Py_int1 = PyLong_FromLong(1);

    return m;
}
