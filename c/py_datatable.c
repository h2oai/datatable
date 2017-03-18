#include "datatable.h"
#include "py_datatable.h"
#include "py_datawindow.h"
#include "py_rowindex.h"

// Forward declarations
void dt_DataTable_dealloc_objcol(void *data, int64_t nrows);



/**
 * "Main" function that drives transformation of datatables.
 *
 * :param rows:
 *     A row selector (a `RowIndex_PyObject` object). This cannot be None --
 *     instead supply row index spanning all rows in the datatable.
 *
 * ... more to be added ...
 */
static DataTable_PyObject*
__call__(DataTable_PyObject *self, PyObject *args, PyObject *kwds)
{
    RowIndex_PyObject *rows = NULL;
    DataTable *dtres = NULL;
    DataTable_PyObject *pyres = NULL;

    static char *kwlist[] = {"rows", NULL};
    int ret = PyArg_ParseTupleAndKeywords(args, kwds, "O!:DataTable.__call__",
                                          kwlist, &RowIndex_PyType, &rows);
    if (!ret || rows->ref == NULL) return NULL;

    dtres = dt_DataTable_call(self->ref, rows->ref);
    if (dtres == NULL) goto fail;
    rows->ref = NULL; // The reference ownership is transferred to `dtres`

    pyres = DataTable_PyNew();
    if (pyres == NULL) goto fail;
    pyres->ref = dtres;
    if (dtres->source == NULL)
        pyres->source = NULL;
    else {
        if (dtres->source == self->ref)
            pyres->source = self;
        else if (dtres->source == self->ref->source)
            pyres->source = self->source;
        else {
            PyErr_SetString(PyExc_RuntimeError, "Unknown source dataframe");
            goto fail;
        }
        Py_XINCREF(pyres->source);
    }

    return pyres;

  fail:
    dt_DataTable_dealloc(dtres, &dt_DataTable_dealloc_objcol);
    Py_XDECREF(pyres);
    return NULL;
}


static PyObject* get_nrows(DataTable_PyObject *self) {
    return PyLong_FromLong(self->ref->nrows);
}

static PyObject* get_ncols(DataTable_PyObject *self) {
    return PyLong_FromLong(self->ref->ncols);
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



/**
 * Deallocator function, called when the object is being garbage-collected.
 */
static void __dealloc__(DataTable_PyObject *self)
{
    dt_DataTable_dealloc(self->ref, &dt_DataTable_dealloc_objcol);
    Py_XDECREF(self->source);
    Py_TYPE(self)->tp_free((PyObject*)self);
}

void dt_DataTable_dealloc_objcol(void *data, int64_t nrows) {
    PyObject **coldata = data;
    int j = nrows;
    while (--j >= 0)
        Py_XDECREF(coldata[j]);
}



//======================================================================================================================
// DataTable type definition
//======================================================================================================================

PyDoc_STRVAR(dtdoc_window, "Retrieve datatable's data within a window");
PyDoc_STRVAR(dtdoc_nrows, "Number of rows in the datatable");
PyDoc_STRVAR(dtdoc_ncols, "Number of columns in the datatable");

#define METHOD1(name) {#name, (PyCFunction)name, METH_VARARGS, dtdoc_##name}

static PyMethodDef datatable_methods[] = {
    METHOD1(window),
    {NULL, NULL}           /* sentinel */
};

static PyGetSetDef datatable_getseters[] = {
    {"nrows", (getter)get_nrows, NULL, dtdoc_nrows, NULL},
    {"ncols", (getter)get_ncols, NULL, dtdoc_ncols, NULL},
    {NULL}  /* sentinel */
};

PyTypeObject DataTable_PyType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "_datatable.DataTable",             /* tp_name */
    sizeof(DataTable_PyObject),         /* tp_basicsize */
    0,                                  /* tp_itemsize */
    (destructor)__dealloc__,            /* tp_dealloc */
    0,                                  /* tp_print */
    0,                                  /* tp_getattr */
    0,                                  /* tp_setattr */
    0,                                  /* tp_compare */
    0,                                  /* tp_repr */
    0,                                  /* tp_as_number */
    0,                                  /* tp_as_sequence */
    0,                                  /* tp_as_mapping */
    0,                                  /* tp_hash  */
    (ternaryfunc)__call__,              /* tp_call */
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
};
