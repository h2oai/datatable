#include <fcntl.h>   // open
#include <unistd.h>  // write, fsync, close
#include "datatable.h"
#include "py_column.h"
#include "py_columnset.h"
#include "py_datatable.h"
#include "py_datawindow.h"
#include "py_rowindex.h"
#include "py_types.h"
#include "py_utils.h"

// Forward declarations
static PyObject *strRowIndexTypeArr32;
static PyObject *strRowIndexTypeArr64;
static PyObject *strRowIndexTypeSlice;


/**
 * Create a new DataTable_PyObject by wrapping the provided DataTable `dt`.
 * If `dt` is NULL then this function also returns NULL.
 */
PyObject* pydt_from_dt(DataTable *dt)
{
    if (dt == NULL) return NULL;
    PyObject *pydt = PyObject_CallObject((PyObject*) &DataTable_PyType, NULL);
    ((DataTable_PyObject*) pydt)->ref = dt;
    return pydt;
}
#define py pydt_from_dt

int dt_unwrap(PyObject *object, void *address) {
    DataTable **ans = address;
    if (!PyObject_TypeCheck(object, &DataTable_PyType)) {
        PyErr_SetString(PyExc_TypeError, "Expected object of type DataTable");
        return 0;
    }
    *ans = ((DataTable_PyObject*)object)->ref;
    return 1;
}


static PyObject* get_nrows(DataTable_PyObject *self) {
    return PyLong_FromLongLong(self->ref->nrows);
}

static PyObject* get_ncols(DataTable_PyObject *self) {
    return PyLong_FromLongLong(self->ref->ncols);
}

static PyObject* get_isview(DataTable_PyObject *self) {
    return incref(self->ref->rowindex == NULL? Py_False : Py_True);
}


static PyObject* get_types(DataTable_PyObject *self)
{
    int64_t i = self->ref->ncols;
    PyObject *list = PyTuple_New((Py_ssize_t) i);
    if (list == NULL) return NULL;
    while (--i >= 0) {
        SType st = self->ref->columns[i]->stype;
        LType lt = stype_info[st].ltype;
        PyTuple_SET_ITEM(list, i, incref(py_ltype_names[lt]));
    }
    return list;
}


static PyObject* get_stypes(DataTable_PyObject *self)
{
    DataTable *dt = self->ref;
    int64_t i = dt->ncols;
    PyObject *list = PyTuple_New((Py_ssize_t) i);
    if (list == NULL) return NULL;
    while (--i >= 0) {
        SType st = dt->columns[i]->stype;
        PyTuple_SET_ITEM(list, i, incref(py_stype_names[st]));
    }
    return list;
}


static PyObject* get_rowindex_type(DataTable_PyObject *self)
{
    if (self->ref->rowindex == NULL)
        return none();
    RowIndexType rit = self->ref->rowindex->type;
    return rit == RI_SLICE? incref(strRowIndexTypeSlice) :
           rit == RI_ARR32? incref(strRowIndexTypeArr32) :
           rit == RI_ARR64? incref(strRowIndexTypeArr64) : none();
}


static PyObject* get_datatable_ptr(DataTable_PyObject *self)
{
    return PyLong_FromLongLong((long long int)self->ref);
}


static DataWindow_PyObject* window(DataTable_PyObject *self, PyObject *args)
{
    int64_t row0, row1, col0, col1;
    if (!PyArg_ParseTuple(args, "llll", &row0, &row1, &col0, &col1))
        return NULL;

    PyObject *nargs = Py_BuildValue("Ollll", self, row0, row1, col0, col1);
    PyObject *res = PyObject_CallObject((PyObject*) &DataWindow_PyType, nargs);
    Py_XDECREF(nargs);

    return (DataWindow_PyObject*) res;
}



PyObject* pydatatable_assemble(UU, PyObject *args)
{
    RowIndex_PyObject *pyri;
    ColumnSet_PyObject *pycols;
    if (!PyArg_ParseTuple(args, "O!O!:datatable_assemble_view",
                          &RowIndex_PyType, &pyri,
                          &ColumnSet_PyType, &pycols))
        return NULL;
    RowIndex *rowindex = pyri->ref;
    pyri->ref = NULL;
    Column **columns = pycols->columns;
    pycols->columns = NULL;
    return py(datatable_assemble(rowindex, columns));
}



PyObject* pydatatable_load(UU, PyObject *args)
{
    DataTable *colspec;
    int64_t nrows;
    if (!PyArg_ParseTuple(args, "O&n:datatable_load",
                          &dt_unwrap, &colspec, &nrows))
        return NULL;
    return py(datatable_load(colspec, nrows));
}



static PyObject*
verify_integrity(DataTable_PyObject *self, PyObject *args)
{
    DataTable *dt = self->ref;
    _Bool fix = 0;

    if (!PyArg_ParseTuple(args, "|b", &fix))
        return NULL;

    char *errors;
    int res = dt_verify_integrity(dt, &errors, fix);
    if (res) {
        return PyUnicode_FromString(errors);
    } else
        return none();
}



static PyObject*
column(DataTable_PyObject *self, PyObject *args)
{
    DataTable *dt = self->ref;
    int64_t colidx;
    if (!PyArg_ParseTuple(args, "l:column", &colidx))
        return NULL;
    if (colidx < 0 || colidx >= dt->ncols) {
        PyErr_Format(PyExc_ValueError, "Invalid column index %lld", colidx);
        return NULL;
    }
    Column_PyObject *pycol = pyColumn_from_Column(dt->columns[colidx]);
    return (PyObject*) pycol;
}



static PyObject* delete_columns(DataTable_PyObject *self, PyObject *args)
{
    DataTable *dt = self->ref;
    PyObject *list;
    if (!PyArg_ParseTuple(args, "O!:delete_columns", &PyList_Type, &list))
        return NULL;

    int ncols = (int) PyList_Size(list);
    int *cols_to_remove = NULL;
    dtmalloc(cols_to_remove, int, ncols);
    for (int i = 0; i < ncols; i++) {
        PyObject *item = PyList_GET_ITEM(list, i);
        cols_to_remove[i] = (int) PyLong_AsLong(item);
    }
    dt_delete_columns(dt, cols_to_remove, ncols);

    dtfree(cols_to_remove);
    return none();
}



static PyObject* rbind(DataTable_PyObject *self, PyObject *args)
{
    DataTable *dt = self->ref;
    int final_ncols;
    PyObject *list;
    if (!PyArg_ParseTuple(args, "iO!:delete_columns",
                          &final_ncols, &PyList_Type, &list))
        return NULL;

    int ndts = (int) PyList_Size(list);
    DataTable **dts = NULL;
    dtmalloc(dts, DataTable*, ndts);
    int **cols_to_append = NULL;
    dtmalloc(cols_to_append, int*, final_ncols);
    for (int i = 0; i < final_ncols; i++) {
        dtmalloc(cols_to_append[i], int, ndts);
    }
    for (int i = 0; i < ndts; i++) {
        PyObject *item = PyList_GET_ITEM(list, i);
        DataTable *dti;
        PyObject *colslist;
        if (!PyArg_ParseTuple(item, "O&O!",
                              &dt_unwrap, &dti, &PyList_Type, &colslist))
            return NULL;
        int ncolsi = (int) PyList_Size(colslist);
        int j = 0;
        for (; j < ncolsi; j++) {
            PyObject *itemj = PyList_GET_ITEM(colslist, j);
            cols_to_append[j][i] = (itemj == Py_None)? -1
                                   : (int) PyLong_AsLong(itemj);
        }
        for (; j < final_ncols; j++) {
            cols_to_append[j][i] = -1;
        }
        dts[i] = dti;
    }
    DataTable *ret = dt_rbind(dt, dts, cols_to_append, ndts, final_ncols);
    if (ret == NULL) return NULL;

    dtfree(cols_to_append);
    dtfree(dts);
    return none();
}


static PyObject* sort(DataTable_PyObject *self, PyObject *args)
{
    DataTable *dt = self->ref;
    int idx;
    if (!PyArg_ParseTuple(args, "i:sort", &idx)) return NULL;

    Column *col = dt->columns[idx];
    if (col->stype != ST_INTEGER_I4) {
        PyErr_SetString(PyExc_ValueError, "Can only sort i4i columns");
        return NULL;
    }

    RowIndex *ri = column_sort(col, dt->nrows);
    return pyrowindex(ri);
}



/**
 * Deallocator function, called when the object is being garbage-collected.
 */
static void _dealloc_(DataTable_PyObject *self)
{
    datatable_dealloc(self->ref);
    Py_TYPE(self)->tp_free((PyObject*)self);
}




//======================================================================================================================
// DataTable type definition
//======================================================================================================================

PyDoc_STRVAR(dtdoc_window, "Retrieve datatable's data within a window");
PyDoc_STRVAR(dtdoc_verify_integrity, "Check and repair the datatable");
PyDoc_STRVAR(dtdoc_nrows, "Number of rows in the datatable");
PyDoc_STRVAR(dtdoc_ncols, "Number of columns in the datatable");
PyDoc_STRVAR(dtdoc_types, "List of column types");
PyDoc_STRVAR(dtdoc_stypes, "List of column storage types");
PyDoc_STRVAR(dtdoc_isview, "Is the datatable view or now?");
PyDoc_STRVAR(dtdoc_rowindex_type, "Type of the row index: 'slice' or 'array'");
PyDoc_STRVAR(dtdoc_column, "Get the requested column in the datatable");
PyDoc_STRVAR(dtdoc_datatable_ptr, "Get pointer (converted to an int) to the wrapped DataTable object");
PyDoc_STRVAR(dtdoc_delete_columns, "Remove the specified list of columns from the datatable");
PyDoc_STRVAR(dtdoc_rbind, "Append rows of other datatables to the current");
PyDoc_STRVAR(dtdoc_sort, "Sort datatable according to a column");

#define METHOD1(name) {#name, (PyCFunction)name, METH_VARARGS, dtdoc_##name}

static PyMethodDef datatable_methods[] = {
    METHOD1(window),
    METHOD1(verify_integrity),
    METHOD1(column),
    METHOD1(delete_columns),
    METHOD1(rbind),
    METHOD1(sort),
    {NULL, NULL, 0, NULL}           /* sentinel */
};

#define GETSET1(name) {#name, (getter)get_##name, NULL, dtdoc_##name, NULL}

static PyGetSetDef datatable_getseters[] = {
    GETSET1(nrows),
    GETSET1(ncols),
    GETSET1(types),
    GETSET1(stypes),
    GETSET1(isview),
    GETSET1(rowindex_type),
    GETSET1(datatable_ptr),
    {NULL, NULL, NULL, NULL, NULL}  /* sentinel */
};

PyTypeObject DataTable_PyType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "_datatable.DataTable",             /* tp_name */
    sizeof(DataTable_PyObject),         /* tp_basicsize */
    0,                                  /* tp_itemsize */
    (destructor)_dealloc_,              /* tp_dealloc */
    0,                                  /* tp_print */
    0,                                  /* tp_getattr */
    0,                                  /* tp_setattr */
    0,                                  /* tp_compare */
    0,                                  /* tp_repr */
    0,                                  /* tp_as_number */
    0,                                  /* tp_as_sequence */
    0,                                  /* tp_as_mapping */
    0,                                  /* tp_hash  */
    0,                                  /* tp_call */
    0,                                  /* tp_str */
    0,                                  /* tp_getattro */
    0,                                  /* tp_setattro */
    0,                                  /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT,                 /* tp_flags */
    "DataTable object",                 /* tp_doc */
    0,                                  /* tp_traverse */
    0,                                  /* tp_clear */
    0,                                  /* tp_richcompare */
    0,                                  /* tp_weaklistoffset */
    0,                                  /* tp_iter */
    0,                                  /* tp_iternext */
    datatable_methods,                  /* tp_methods */
    0,                                  /* tp_members */
    datatable_getseters,                /* tp_getset */
    0,                                  /* tp_base */
    0,                                  /* tp_dict */
    0,                                  /* tp_descr_get */
    0,                                  /* tp_descr_set */
    0,                                  /* tp_dictoffset */
    0,                                  /* tp_init */
    0,                                  /* tp_alloc */
    0,                                  /* tp_new */
    0,                                  /* tp_free */
    0,                                  /* tp_is_gc */
    0,                                  /* tp_bases */
    0,                                  /* tp_mro */
    0,                                  /* tp_cache */
    0,                                  /* tp_subclasses */
    0,                                  /* tp_weaklist */
    0,                                  /* tp_del */
    0,                                  /* tp_version_tag */
    0,                                  /* tp_finalize */
};


int init_py_datatable(PyObject *module) {
    // Register DataTable_PyType on the module
    DataTable_PyType.tp_new = PyType_GenericNew;
    if (PyType_Ready(&DataTable_PyType) < 0) return 0;
    Py_INCREF(&DataTable_PyType);
    PyModule_AddObject(module, "DataTable", (PyObject*) &DataTable_PyType);

    strRowIndexTypeArr32 = PyUnicode_FromString("arr32");
    strRowIndexTypeArr64 = PyUnicode_FromString("arr64");
    strRowIndexTypeSlice = PyUnicode_FromString("slice");
    return 1;
}
