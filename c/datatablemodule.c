#include <Python.h>
#include "py_column.h"
#include "py_columnset.h"
#include "py_datatable.h"
#include "py_datawindow.h"
#include "py_rowmapping.h"
#include "py_types.h"
#include "py_utils.h"
#include "fread_impl.h"

#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>   // for open()

PyMODINIT_FUNC PyInit__datatable(void);

// where should this be moved?
PyObject *dt_from_memmap(PyObject *self, PyObject *args);
PyObject* exec_function(PyObject *self, PyObject *args);


PyObject *dt_from_memmap(UU, PyObject *args)
{
    PyObject *list;

    if (!PyArg_ParseTuple(args, "O!", &PyList_Type, &list))
        return NULL;

    int64_t ncols = PyList_Size(list);

    DataTable *dt = NULL;
    dtmalloc(dt, DataTable, 1);
    dt->ncols = ncols;
    dt->nrows = 0;
    dt->rowmapping = NULL;
    dtmalloc(dt->columns, Column*, ncols + 1);
    dt->columns[ncols] = NULL;

    int64_t nrows = -1;
    for (int i = 0; i < ncols; i++) {
        PyObject *item = PyList_GetItem(list, i);
        char *colname = PyUnicode_AsUTF8(item);
        char *dotptr = strrchr(colname, '.');
        if (dotptr == NULL) {
            PyErr_Format(PyExc_ValueError, "Cannot find . in column %s", colname);
            return NULL;
        }
        SType elemtype = strcmp(dotptr + 1, "bool") == 0? ST_BOOLEAN_I1 :
                             strcmp(dotptr + 1, "int64") == 0? ST_INTEGER_I8 :
                             strcmp(dotptr + 1, "double") == 0? ST_REAL_F8 : 0;
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
        if (filesize > INTPTR_MAX) {
            close(fd);
            PyErr_Format(PyExc_ValueError,
                "File is too big for a 32-bit platform: %lld bytes", filesize);
            return NULL;
        }
        void *mmp = mmap(NULL, (size_t)filesize, PROT_READ, MAP_PRIVATE, fd, 0);
        if (mmp == MAP_FAILED) {
            close(fd);
            PyErr_Format(PyExc_RuntimeError, "Failed to memory-map the file: %s", colname);
            return NULL;
        }
        // After memory-mapping the file, its descriptor is no longer needed
        close(fd);

        int64_t nelems = (int64_t)filesize / (int64_t)elemsize;
        if (nrows == -1)
            nrows = nelems;
        else if (nrows != nelems) {
            PyErr_Format(PyExc_RuntimeError,
                "File %s contains %zd rows, whereas previous column(s) had %zd rows",
                colname, nelems, nrows);
            return NULL;
        }

        dtmalloc(dt->columns[i], Column, 1);
        dt->columns[i]->data = mmp;
        dt->columns[i]->stype = elemtype;
        dt->columns[i]->mtype = MT_MMAP;
        dt->columns[i]->meta = NULL;
        dt->columns[i]->alloc_size = (size_t) filesize;
        dt->columns[i]->refcount = 1;
    }

    dt->nrows = nrows;
    return pydt_from_dt(dt);
}

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
    METHOD0(datatable_assemble),
    METHOD0(rowmapping_from_slice),
    METHOD0(rowmapping_from_slicelist),
    METHOD0(rowmapping_from_array),
    METHOD0(rowmapping_from_column),
    METHOD0(rowmapping_from_filterfn),
    METHOD0(datatable_from_list),
    {"fread", (PyCFunction)freadPy, METH_VARARGS,
        "Read a text file and convert into a datatable"},
    {"dt_from_memmap", (PyCFunction)dt_from_memmap, METH_VARARGS,
        "Load DataTable from the pdt files"},
    {"write_column_to_file", (PyCFunction)write_column_to_file, METH_VARARGS,
        "Write a single Column to a file on disk"},
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

static_assert(sizeof(int64_t) == sizeof(Py_ssize_t),
              "int64_t and Py_ssize_t should refer to the same type");

/* Called when Python program imports the module */
PyMODINIT_FUNC
PyInit__datatable(void) {
    // Sanity checks
    assert(sizeof(char) == sizeof(unsigned char));
    assert(sizeof(void) == 1);
    assert('\0' == (char)0);
    // Used in llvm.py
    assert(sizeof(long long int) == sizeof(int64_t));
    assert(sizeof(SType) == sizeof(int));
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
    if (!init_py_types(m)) return NULL;
    if (!init_py_column(m)) return NULL;
    if (!init_py_columnset(m)) return NULL;

    return m;
}
