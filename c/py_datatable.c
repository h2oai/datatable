#include "datatable.h"
#include "py_datatable.h"
#include "py_datawindow.h"
#include "py_rowmapping.h"
#include "dtutils.h"

// Forward declarations
void dt_DataTable_dealloc_objcol(void *data, int64_t nrows);

static PyObject *strRowMappingTypeArray, *strRowMappingTypeSlice;

PyObject **py_string_coltypes;


int init_py_datatable(PyObject *module) {
    // Register DataTable_PyType on the module
    DataTable_PyType.tp_new = PyType_GenericNew;
    if (PyType_Ready(&DataTable_PyType) < 0) return 0;
    Py_INCREF(&DataTable_PyType);
    PyModule_AddObject(module, "DataTable", (PyObject*) &DataTable_PyType);

    // initialize auxiliary data
    py_string_coltypes = malloc(sizeof(PyObject*) * DT_COUNT);
    py_string_coltypes[DT_AUTO]   = PyUnicode_FromString("auto");
    py_string_coltypes[DT_DOUBLE] = PyUnicode_FromString("real");
    py_string_coltypes[DT_LONG]   = PyUnicode_FromString("int");
    py_string_coltypes[DT_BOOL]   = PyUnicode_FromString("bool");
    py_string_coltypes[DT_STRING] = PyUnicode_FromString("str");
    py_string_coltypes[DT_OBJECT] = PyUnicode_FromString("obj");
    strRowMappingTypeArray = PyUnicode_FromString("array");
    strRowMappingTypeSlice = PyUnicode_FromString("slice");
    return 1;
}


/**
 * "Main" function that drives transformation of datatables.
 *
 * :param rows:
 *     A row selector (a `RowMapping_PyObject` object). This cannot be None --
 *     instead supply row index spanning all rows in the datatable.
 *
 * ... more to be added ...
 */
static DataTable_PyObject*
__call__(DataTable_PyObject *self, PyObject *args, PyObject *kwds)
{
    RowMapping_PyObject *rows = NULL;
    DataTable *dtres = NULL;
    DataTable_PyObject *pyres = NULL;

    static char *kwlist[] = {"rows", NULL};
    int ret = PyArg_ParseTupleAndKeywords(args, kwds, "O!:DataTable.__call__",
                                          kwlist, &RowMapping_PyType, &rows);
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

static PyObject* get_isview(DataTable_PyObject *self) {
    return incref(self->ref->source == NULL? Py_False : Py_True);
}

static PyObject* get_types(DataTable_PyObject *self)
{
    int64_t i = self->ref->ncols;
    PyObject *list = PyTuple_New((Py_ssize_t) i);
    if (list == NULL) return NULL;
    while (--i >= 0) {
        ColType ct = self->ref->columns[i].type;
        PyTuple_SET_ITEM(list, i, incref(py_string_coltypes[ct]));
    }
    return list;
}

static PyObject* get_rowindex_type(DataTable_PyObject *self)
{
    if (self->ref->rowmapping == NULL)
        return none();
    RowMappingType rit = self->ref->rowmapping->type;
    return rit == RI_SLICE? incref(strRowMappingTypeSlice) :
           rit == RI_ARRAY? incref(strRowMappingTypeArray) : none();
}

/**
 * If the datatable is a view, then return the tuple of source column numbers
 * for all columns in the current datatable. That is, we return the tuple
 *     tuple(col.srcindex  for col in self.columns)
 * If any column contains computed data, then its "index" will be returned
 * as None.
 * If the datatable is not a view, return None.
 */
static PyObject* get_view_colnumbers(DataTable_PyObject *self)
{
    if (self->ref->source == NULL)
        return none();
    int64_t i = self->ref->ncols;
    Column *columns = self->ref->columns;
    PyObject *list = PyTuple_New((Py_ssize_t) i);
    if (list == NULL) return NULL;
    while (--i >= 0) {
        int isdatacol = columns[i].data == NULL;
        int64_t srcindex = columns[i].srcindex;
        PyObject *idx = isdatacol? PyLong_FromLong(srcindex) : none();
        PyTuple_SET_ITEM(list, i, idx);
    }
    return list;
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
    int64_t j = nrows;
    while (--j >= 0)
        Py_XDECREF(coldata[j]);
}


static PyObject* test(DataTable_PyObject *self, PyObject *args)
{
    void *ptr;
    if (!PyArg_ParseTuple(args, "l", &ptr))
        return NULL;

    DataTable *dt = self->ref;
    int64_t *buf = calloc(sizeof(int64_t), dt->nrows);

    int64_t (*func)(DataTable*, int64_t*) = ptr;
    int64_t res = func(dt, buf);

    PyObject *list = PyList_New(res);
    for (int64_t i = 0; i < res; i++) {
        PyList_SET_ITEM(list, i, PyLong_FromLong(buf[i]));
    }
    free(buf);
    return list;
}


//======================================================================================================================
// DataTable type definition
//======================================================================================================================

PyDoc_STRVAR(dtdoc_window, "Retrieve datatable's data within a window");
PyDoc_STRVAR(dtdoc_nrows, "Number of rows in the datatable");
PyDoc_STRVAR(dtdoc_ncols, "Number of columns in the datatable");
PyDoc_STRVAR(dtdoc_types, "List of column types");
PyDoc_STRVAR(dtdoc_isview, "Is the datatable view or now?");
PyDoc_STRVAR(dtdoc_rowindex_type, "Type of the row numbers: 'slice' or 'array'");
PyDoc_STRVAR(dtdoc_view_colnumbers, "List of source column indices in a view");
PyDoc_STRVAR(dtdoc_test, "");

#define METHOD1(name) {#name, (PyCFunction)name, METH_VARARGS, dtdoc_##name}

static PyMethodDef datatable_methods[] = {
    METHOD1(window),
    METHOD1(test),
    {NULL, NULL}           /* sentinel */
};

#define GETSET1(name) {#name, (getter)get_##name, NULL, dtdoc_##name, NULL}

static PyGetSetDef datatable_getseters[] = {
    GETSET1(nrows),
    GETSET1(ncols),
    GETSET1(types),
    GETSET1(isview),
    GETSET1(rowindex_type),
    GETSET1(view_colnumbers),
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
