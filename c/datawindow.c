#include <Python.h>
#include "datatable.h"
#include "datawindow.h"
#include "dtutils.h"
#include "structmember.h"


static int __init__(dt_DataWindowObject *self, PyObject *args, PyObject *kwds)
{
    PyObject *types = NULL, *view = NULL;

    // Parse arguments and check their validity
    dt_DatatableObject *dt;
    long view_col0, view_ncols, view_row0, view_nrows;
    if (!PyArg_ParseTuple(args, "O!llll:DataWindow.__init__",
                          &dt_DatatableType, &dt,
                          &view_col0, &view_ncols, &view_row0, &view_nrows))
        return -1;

    if (view_col0 < 0 || view_col0 + view_ncols > dt->ncols ||
        view_row0 < 0 || view_row0 + view_nrows > dt->nrows) {
        PyErr_SetString(PyExc_ValueError, "Invalid data window bounds");
        return -1;
    }

    // Create and fill-in the `types` list
    types = PyList_New((Py_ssize_t) view_ncols);
    if (types == NULL) goto fail;
    for (long i = 0; i < view_ncols; ++i) {
        dt_Column column = dt->columns[view_col0 + i];
        PyObject *py_coltype = PyLong_FromLong(column.type);
        if (py_coltype == NULL) {
            // Set all remaining list elements to null, o/w Python will attempt
            // to free() them, which will likely crash
            for (; i < view_ncols; ++i) PyList_SET_ITEM(types, i, NULL);
            goto fail;
        }
        PyList_SET_ITEM(types, i, py_coltype);
    }

    dt_RowsIndexObject *rindex = dt->row_index;
    int indirect_array = rindex && rindex->kind == RI_ARRAY;

    // Create and fill-in the `data` list
    view = PyList_New((Py_ssize_t) view_ncols);
    if (view == NULL) goto fail;
    for (long i = 0; i < view_ncols; ++i) {
        dt_Column column = dt->columns[view_col0 + i];
        int indirect = (column.data == NULL);
        // Note that we do not allow double-indirection here...
        void *coldata = indirect? dt->src->columns[column.index].data
                                : column.data;

        PyObject *py_coldata = PyList_New((Py_ssize_t) view_nrows);
        if (py_coldata == NULL) {
            for (; i < view_ncols; ++i) PyList_SET_ITEM(view, i, NULL);
            goto fail;
        }
        PyList_SET_ITEM(view, i, py_coldata);
        PyObject *value;
        for (long j = 0; j < view_nrows; ++j) {
            long irow = view_row0 + j;
            if (indirect) {
                irow = indirect_array? rindex->riArray.rows[irow]
                                     : rindex->riSlice.start +
                                       rindex->riSlice.step * irow;
            }
            switch (column.type) {
                case DT_DOUBLE: {
                    double x = ((double*)coldata)[irow];
                    value = isnan(x)? none() : PyFloat_FromDouble(x);
                }   break;

                case DT_LONG: {
                    long x = ((long*)coldata)[irow];
                    value = x == LONG_MIN? none() : PyLong_FromLong(x);
                }   break;

                case DT_STRING: {
                    char* x = ((char**)coldata)[irow];
                    value = x == NULL? none() : PyUnicode_FromString(x);
                }   break;

                case DT_BOOL: {
                    unsigned char x = ((char*)coldata)[irow];
                    value = x == 0? Py_int0 : x == 1? Py_int1 : Py_None;
                    Py_INCREF(value);
                }   break;

                case DT_OBJECT: {
                    value = ((PyObject**)coldata)[irow];
                    Py_INCREF(value);
                }   break;

                default:
                    printf("Unknown column type: %d\n", column.type);
                    printf("indirect = %d\n", indirect);
                    printf("coldata = %p\n", coldata);
                    printf("rowindex = %p\n", rindex);
                    printf("rowindex kind = %d\n", rindex? rindex->kind : -1);
                    printf("dtsrc = %p\n", dt->src);
                    PyErr_SetString(PyExc_RuntimeError, "Unknown column type");
                    goto fail;
            }
            if (value == NULL) {
                for (; j < view_nrows; ++j) PyList_SET_ITEM(py_coldata, j, NULL);
                goto fail;
            }
            PyList_SET_ITEM(py_coldata, j, value);
        }
    }

    Py_XDECREF(self->types);  // Just in case these were already initialized
    Py_XDECREF(self->data);
    self->col0 = view_col0;
    self->ncols = view_ncols;
    self->row0 = view_row0;
    self->nrows = view_nrows;
    self->types = types;
    self->data = view;
    return 0;

fail:
    Py_XDECREF(view);
    Py_XDECREF(types);
    return -1;
}


static void __dealloc__(dt_DataWindowObject *self)
{
    Py_XDECREF(self->data);
    Py_XDECREF(self->types);
    Py_TYPE(self)->tp_free((PyObject*)self);
}


//------ Declare the DataWindow object -----------------------------------------

PyDoc_STRVAR(dtdoc_datawindow, "DataWindow object");
PyDoc_STRVAR(dtdoc_ncols, "Number of columns");
PyDoc_STRVAR(dtdoc_col0, "Index of the first column");
PyDoc_STRVAR(dtdoc_nrows, "Number of rows");
PyDoc_STRVAR(dtdoc_row0, "Index of the first row");
PyDoc_STRVAR(dtdoc_types, "Types of the columns within the view");
PyDoc_STRVAR(dtdoc_data, "Datatable's data within the specified window");


#define Member(name, type) {#name, type, offsetof(dt_DataWindowObject, name), \
                            READONLY, dtdoc_ ## name}

static PyMemberDef members[] = {
    Member(col0, T_LONG),
    Member(ncols, T_LONG),
    Member(row0, T_LONG),
    Member(nrows, T_LONG),
    Member(types, T_OBJECT_EX),
    Member(data, T_OBJECT_EX),
    {NULL}  // sentinel
};


PyTypeObject dt_DataWindowType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "_datatable.DataWindow",            /* tp_name */
    sizeof(dt_DataWindowObject),        /* tp_basicsize */
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
    0,                                  /* tp_call */
    0,                                  /* tp_str */
    0,                                  /* tp_getattro */
    0,                                  /* tp_setattro */
    0,                                  /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT,                 /* tp_flags */
    dtdoc_datawindow,                   /* tp_doc */
    0,                                  /* tp_traverse */
    0,                                  /* tp_clear */
    0,                                  /* tp_richcompare */
    0,                                  /* tp_weaklistoffset */
    0,                                  /* tp_iter */
    0,                                  /* tp_iternext */
    0,                                  /* tp_methods */
    members,                            /* tp_members */
    0,                                  /* tp_getset */
    0,                                  /* tp_base */
    0,                                  /* tp_dict */
    0,                                  /* tp_descr_get */
    0,                                  /* tp_descr_set */
    0,                                  /* tp_dictoffset */
    (initproc)__init__,                 /* tp_init */
};
