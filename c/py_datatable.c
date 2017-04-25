#include <fcntl.h>   // open
#include <unistd.h>  // write, fsync, close
#include "datatable.h"
#include "py_colmapping.h"
#include "py_datatable.h"
#include "py_datawindow.h"
#include "py_rowmapping.h"
#include "py_types.h"
#include "py_utils.h"

// Forward declarations
void dt_DataTable_dealloc_objcol(void *data, int64_t nrows);

static PyObject *strRowMappingTypeArray;
static PyObject *strRowMappingTypeSlice;


int init_py_datatable(PyObject *module) {
    // Register DataTable_PyType on the module
    DataTable_PyType.tp_new = PyType_GenericNew;
    if (PyType_Ready(&DataTable_PyType) < 0) return 0;
    Py_INCREF(&DataTable_PyType);
    PyModule_AddObject(module, "DataTable", (PyObject*) &DataTable_PyType);

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
 * :param cols:
 *     A column selector (:class:`ColMapping_PyObject`).
 *
 * ... more to be added ...
 */
static DataTable_PyObject*
__call__(DataTable_PyObject *self, PyObject *args, PyObject *kwds)
{
    RowMapping_PyObject *rows = NULL;
    ColMapping_PyObject *cols = NULL;
    DataTable *dtres = NULL;
    DataTable_PyObject *pyres = NULL;

    static char *kwlist[] = {"rows", "cols", NULL};
    int ret = PyArg_ParseTupleAndKeywords(args, kwds,
        "O!O!:DataTable.__call__", kwlist,
        &RowMapping_PyType, &rows, &ColMapping_PyType, &cols
    );
    if (!ret || rows->ref == NULL || cols->ref == NULL) return NULL;

    dtres = dt_DataTable_call(self->ref, rows->ref, cols->ref);
    if (dtres == NULL) goto fail;
    rows->ref = NULL;  // The reference ownership is transferred to `dtres`

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


int dt_from_pydt(PyObject *object, void *address) {
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
    return incref(self->ref->source == NULL? Py_False : Py_True);
}


static PyObject* get_types(DataTable_PyObject *self)
{
    int64_t i = self->ref->ncols;
    PyObject *list = PyTuple_New((Py_ssize_t) i);
    if (list == NULL) return NULL;
    while (--i >= 0) {
        SType st = self->ref->columns[i].stype;
        LType lt = stype_info[st].ltype;
        PyTuple_SET_ITEM(list, i, incref(py_ltype_names[lt]));
    }
    return list;
}


static PyObject* get_stypes(DataTable_PyObject *self)
{
    int64_t i = self->ref->ncols;
    PyObject *list = PyTuple_New((Py_ssize_t) i);
    if (list == NULL) return NULL;
    while (--i >= 0) {
        SType st = self->ref->columns[i].stype;
        PyTuple_SET_ITEM(list, i, incref(py_stype_names[st]));
    }
    return list;
}


static PyObject* get_rowmapping_type(DataTable_PyObject *self)
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
        PyObject *idx = isdatacol? PyLong_FromLongLong(srcindex) : none();
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


DataTable_PyObject* pyDataTable_from_DataTable(DataTable *dt)
{
    DataTable_PyObject *pydt = DataTable_PyNew();
    if (pydt == NULL) return NULL;
    if (dt->source != NULL) {
        PyErr_SetString(PyExc_RuntimeError, "Cannot wrap a view datatable");
        return NULL;
    }

    pydt->ref = dt;
    pydt->source = NULL;
    return pydt;
}


PyObject* write_column_to_file(PyObject *self, PyObject *args)
{
    const char *filename = NULL;
    DataTable *dt = NULL;
    int64_t colidx = -1;

    if (!PyArg_ParseTuple(args, "sO&l:write_column_to_file",
        &filename, &dt_from_pydt, &dt, &colidx))
        return NULL;

    if (colidx < 0 || colidx >= dt->ncols) {
        PyErr_Format(PyExc_ValueError, "Invalid column index %lld", colidx);
        return NULL;
    }

    Column *col = &dt->columns[colidx];
    int fd = creat(filename, 0666);
    if (fd == -1) {
        PyErr_Format(PyExc_RuntimeError,
                     "Error %d when opening file %s", errno, filename);
        return NULL;
    }

    if (col->data) {
        // NOTE: write() is unable to write more than 2^32 bytes at once
        const int buff_size = 1 << 20;
        int stype = col->stype;
        int64_t total_size = stype_info[stype].elemsize * dt->nrows;
        if (stype == ST_STRING_I4_VCHAR || stype == ST_STRING_I8_VCHAR) {
            total_size += ((VarcharMeta*)col->meta)->offoff;
        }
        int64_t bytes_written = 0;
        const void *ptr = col->data;
        while (bytes_written < total_size) {
            int64_t nbytes = min(total_size - bytes_written, buff_size);
            ssize_t written = write(fd, ptr, (size_t)nbytes);
            if (written <= 0) {
                PyErr_Format(PyExc_RuntimeError,
                             "Error %d when writing to file", errno);
                return NULL;
            }
            ptr += written;
            bytes_written += written;
        }
    } else {
        PyErr_Format(PyExc_RuntimeError,
                     "Unable to store a view column %lld", colidx);
        return NULL;
    }

    // Done writing
    fsync(fd);
    close(fd);

    return none();
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



//======================================================================================================================
// DataTable type definition
//======================================================================================================================

PyDoc_STRVAR(dtdoc_window, "Retrieve datatable's data within a window");
PyDoc_STRVAR(dtdoc_nrows, "Number of rows in the datatable");
PyDoc_STRVAR(dtdoc_ncols, "Number of columns in the datatable");
PyDoc_STRVAR(dtdoc_types, "List of column types");
PyDoc_STRVAR(dtdoc_stypes, "List of column storage types");
PyDoc_STRVAR(dtdoc_isview, "Is the datatable view or now?");
PyDoc_STRVAR(dtdoc_rowmapping_type, "Type of the row mapping: 'slice' or 'array'");
PyDoc_STRVAR(dtdoc_view_colnumbers, "List of source column indices in a view");

#define METHOD1(name) {#name, (PyCFunction)name, METH_VARARGS, dtdoc_##name}

static PyMethodDef datatable_methods[] = {
    METHOD1(window),
    {NULL, NULL, 0, NULL}           /* sentinel */
};

#define GETSET1(name) {#name, (getter)get_##name, NULL, dtdoc_##name, NULL}

static PyGetSetDef datatable_getseters[] = {
    GETSET1(nrows),
    GETSET1(ncols),
    GETSET1(types),
    GETSET1(stypes),
    GETSET1(isview),
    GETSET1(rowmapping_type),
    GETSET1(view_colnumbers),
    {NULL, NULL, NULL, NULL, NULL}  /* sentinel */
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
    0,                                  /* tp_finalize */
};
