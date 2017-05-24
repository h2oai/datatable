#include <fcntl.h>   // open
#include <unistd.h>  // write, fsync, close
#include "datatable.h"
#include "py_column.h"
#include "py_columnset.h"
#include "py_datatable.h"
#include "py_datawindow.h"
#include "py_rowmapping.h"
#include "py_types.h"
#include "py_utils.h"

// Forward declarations
static PyObject *strRowMappingTypeArr32;
static PyObject *strRowMappingTypeArr64;
static PyObject *strRowMappingTypeSlice;


/**
 * Create a new DataTable_PyObject by wrapping the provided DataTable `dt`.
 * If `dt` is a view, then the source `DataTable_PyObject` must also be given.
 * The returned object will assume ownership of the datatable `dt`. If `dt`
 * is NULL then this function also returns NULL.
 */
PyObject* pydt_from_dt(DataTable *dt, DataTable_PyObject *src)
{
    if (dt == NULL) return NULL;
    if (dt->source != NULL && src == NULL) {
        PyErr_SetString(PyExc_RuntimeError, "Cannot wrap a view datatable");
        return NULL;
    }
    PyObject *pydt = PyObject_CallObject((PyObject*) &DataTable_PyType, NULL);
    ((DataTable_PyObject*) pydt)->ref = dt;
    ((DataTable_PyObject*) pydt)->source = src;
    Py_XINCREF(src);
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
    return incref(self->ref->source == NULL? Py_False : Py_True);
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


static PyObject* get_rowmapping_type(DataTable_PyObject *self)
{
    if (self->ref->rowmapping == NULL)
        return none();
    RowMappingType rmt = self->ref->rowmapping->type;
    return rmt == RM_SLICE? incref(strRowMappingTypeSlice) :
           rmt == RM_ARR32? incref(strRowMappingTypeArr32) :
           rmt == RM_ARR64? incref(strRowMappingTypeArr64) : none();
}


static PyObject* get_datatable_ptr(DataTable_PyObject *self)
{
    return PyLong_FromLongLong((long long int)self->ref);
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
    Column **columns = self->ref->columns;
    PyObject *list = PyTuple_New((Py_ssize_t) i);
    if (list == NULL) return NULL;
    while (--i >= 0) {
        int isviewcol = columns[i]->mtype == MT_VIEW;
        PyObject *idx = isviewcol
            ? PyLong_FromLong(((ViewColumn*)columns[i])->srcindex)
            : none();
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



PyObject* pydatatable_assemble(PyObject *self, PyObject *args)
{
    int64_t nrows;
    Column **cols;
    if (!PyArg_ParseTuple(args, "L!O&:datatable_assemble",
                          &nrows, &columnset_unwrap, &cols))
        return NULL;
    return py(datatable_assemble(nrows, cols), NULL);
}



PyObject* pydatatable_assemble_view(PyObject *self, PyObject *args)
{
    DataTable_PyObject *srcdt;
    RowMapping_PyObject *pyrwm;
    ColumnSet_PyObject *pycols;
    if (!PyArg_ParseTuple(args, "O!O!O!:datatable_assemble_view",
                          &DataTable_PyType, &srcdt,
                          &RowMapping_PyType, &pyrwm,
                          &ColumnSet_PyType, &pycols))
        return NULL;
    RowMapping *rowmapping = pyrwm->ref;
    pyrwm->ref = NULL;
    Column **columns = pycols->columns;
    pycols->columns = NULL;
    return py(datatable_assemble_view(srcdt->ref, rowmapping, columns), srcdt);
}



PyObject* write_column_to_file(PyObject *self, PyObject *args)
{
    const char *filename = NULL;
    DataTable *dt = NULL;
    int64_t colidx = -1;

    if (!PyArg_ParseTuple(args, "sO&l:write_column_to_file",
        &filename, &dt_unwrap, &dt, &colidx))
        return NULL;

    if (colidx < 0 || colidx >= dt->ncols) {
        PyErr_Format(PyExc_ValueError, "Invalid column index %lld", colidx);
        return NULL;
    }

    Column *col = dt->columns[colidx];
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
        int64_t total_size = (int64_t)stype_info[stype].elemsize * dt->nrows;
        if (stype == ST_STRING_I4_VCHAR || stype == ST_STRING_I8_VCHAR) {
            total_size += ((VarcharMeta*)col->meta)->offoff;
        }
        int64_t bytes_written = 0;
        const void *ptr = col->data;
        while (bytes_written < total_size) {
            int64_t nbytes = min(total_size - bytes_written, buff_size);
            int64_t written = write(fd, ptr, (size_t)nbytes);
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



/**
 * Deallocator function, called when the object is being garbage-collected.
 */
static void _dealloc_(DataTable_PyObject *self)
{
    datatable_dealloc(self->ref);
    Py_XDECREF(self->source);
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
PyDoc_STRVAR(dtdoc_rowmapping_type, "Type of the row mapping: 'slice' or 'array'");
PyDoc_STRVAR(dtdoc_view_colnumbers, "List of source column indices in a view");
PyDoc_STRVAR(dtdoc_column, "Get the requested column in the datatable");
PyDoc_STRVAR(dtdoc_datatable_ptr, "Get pointer (converted to an int) to the wrapped DataTable object");

#define METHOD1(name) {#name, (PyCFunction)name, METH_VARARGS, dtdoc_##name}

static PyMethodDef datatable_methods[] = {
    METHOD1(window),
    METHOD1(verify_integrity),
    METHOD1(column),
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

    strRowMappingTypeArr32 = PyUnicode_FromString("arr32");
    strRowMappingTypeArr64 = PyUnicode_FromString("arr64");
    strRowMappingTypeSlice = PyUnicode_FromString("slice");
    return 1;
}
