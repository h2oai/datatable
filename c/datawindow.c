#include <Python.h>
#include "datatable.h"
#include "datawindow.h"
#include "dtutils.h"
#include "structmember.h"

// Forward declarations
static int _check_consistency(
    dt_DatatableObject *dt, long col0, long ncols, long row0, long nrows);


/**
 * DataWindow object constructor. This constructor takes a datatable, and
 * coordinates of a data window, and extracts the data from the datatable
 * within the provided window as a pythonic list of lists.
 *
 * Keyword arguments are not supported (and ignored).
 *
 * :param dt: the datatable whose data window we want to extract.
 * :param col0: index of the first column of the data window.
 * :param ncols: the width of the data window (in columns).
 * :param row0: index of the first row of the data window.
 * :param nrows: the height of the data window (in rows).
 */
static int __init__(dt_DataWindowObject *self, PyObject *args, PyObject *kwds)
{
    PyObject *types = NULL, *view = NULL;
    assert(self->types == NULL && self->data == NULL);

    // Parse arguments and check their validity
    dt_DatatableObject *dt;
    long view_col0, view_ncols, view_row0, view_nrows;
    if (!PyArg_ParseTuple(args, "O!llll:DataWindow.__init__",
                          &dt_DatatableType, &dt,
                          &view_col0, &view_ncols, &view_row0, &view_nrows))
        return -1;
    if (!_check_consistency(dt, view_col0, view_ncols, view_row0, view_nrows))
        return -1;

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

    dt_RowIndexObject *rindex = dt->row_index;
    int indirect_array = rindex && rindex->kind == RI_ARRAY;

    // Create and fill-in the `data` list
    view = PyList_New((Py_ssize_t) view_ncols);
    if (view == NULL) goto fail;
    for (long i = 0; i < view_ncols; ++i) {
        dt_Column column = dt->columns[view_col0 + i];
        int indirect = (column.data == NULL);
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
                irow = indirect_array? rindex->array[irow]
                                     : rindex->slice.start +
                                       rindex->slice.step * irow;
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
                    char x = ((char*)coldata)[irow];
                    value = x == 0? Py_int0 : x == 1? Py_int1 : Py_None;
                    Py_INCREF(value);
                }   break;

                case DT_OBJECT: {
                    value = ((PyObject**)coldata)[irow];
                    Py_INCREF(value);
                }   break;

                default:
                    assert(0);
            }
            PyList_SET_ITEM(py_coldata, j, value);
        }
    }

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


/**
 * This function meticulously checks the supplied datatable for internal
 * consistency, and raises an informative error if any problem is found.
 * Strictly speaking, this function is normally not needed if the datatable
 * class is implemented properly; but it is also relatively cheap to do these
 * checks on each datawindow request, so why not... We can always decide to
 * turn this off in production.
 *
 * :param dt: reference to a datatable being inspected
 * :param col0: index of the first column to check
 * :param ncols: number of columns to check
 * :param row0: index of the first row to check
 * :param nrows: number of rows to check
 * :returns: 1 on success, 0 on failure
 */
static int _check_consistency(
    dt_DatatableObject *dt, long col0, long ncols, long row0, long nrows)
{
    // check correctness of the data window
    if (col0 < 0 || ncols < 0 || col0 + ncols > dt->ncols ||
        row0 < 0 || nrows < 0 || row0 + nrows > dt->nrows) {
        PyErr_Format(PyExc_ValueError,
            "Invalid data window bounds: datatable is [%ld x %ld], "
            "whereas requested window is [%ld..+%ld x %ld..+%ld]",
            dt->nrows, dt->ncols, row0, nrows, col0, ncols);
        return 0;
    }

    // verify that the datatable is internally consistent
    dt_RowIndexObject *rindex = dt->row_index;
    if (rindex == NULL && dt->src != NULL) {
        PyErr_SetString(PyExc_RuntimeError,
            "Invalid datatable: .src is present, but .row_index is null");
        return 0;
    }
    if (rindex != NULL && dt->src == NULL) {
        PyErr_SetString(PyExc_RuntimeError,
            "Invalid datatable: .src is null, while .row_index is present");
        return 0;
    }
    if (dt->src != NULL && dt->src->src != NULL) {
        PyErr_SetString(PyExc_RuntimeError,
            "Invalid view: must not have another view as a parent");
        return 0;
    }

    // verify that the row index (if present) is valid
    if (rindex != NULL) {
        switch (rindex->kind) {
            case RI_ARRAY: {
                if (rindex->length != dt->nrows) {
                    PyErr_Format(PyExc_RuntimeError,
                        "Invalid view: row index has %ld elements, while the "
                        "view itself has .nrows = %ld",
                        rindex->length, dt->nrows);
                    return 0;
                }
                for (long j = 0; j < nrows; ++j) {
                    long irow = row0 + j;
                    long irowsrc = rindex->array[irow];
                    if (irowsrc < 0 || irowsrc >= dt->src->nrows) {
                        PyErr_Format(PyExc_RuntimeError,
                            "Invalid view: row %ld of the view references non-"
                            "existing row %ld in the parent datatable",
                            irow, irowsrc);
                        return 0;
                    }
                }
            }   break;

            case RI_SLICE: {
                long start = rindex->slice.start;
                long count = rindex->length;
                long finish = start + (count - 1) * rindex->slice.step;
                if (count != dt->nrows) {
                    PyErr_Format(PyExc_RuntimeError,
                        "Invalid view: row index has %ld elements, while the "
                        "view itself has .nrows = %ld", count, dt->nrows);
                    return 0;
                }
                if (start < 0 || start >= dt->src->nrows) {
                    PyErr_Format(PyExc_RuntimeError,
                        "Invalid view: first row references an invalid row "
                        "%ld in the parent datatable", start);
                    return 0;
                }
                if (finish < 0 || finish >= dt->src->nrows) {
                    PyErr_Format(PyExc_RuntimeError,
                        "Invalid view: last row references an invalid row "
                        "%ld in the parent datatable", finish);
                    return 0;
                }
            }   break;

            default:
                PyErr_Format(PyExc_RuntimeError,
                    "Unexpected row index kind = %d", rindex->kind);
                return 0;
        }
    }

    // check each column within the window for correctness
    for (long i = 0; i < ncols; ++i) {
        long icol = col0 + i;
        dt_Column col = dt->columns[icol];
        if (col.type == DT_AUTO) {
            PyErr_Format(PyExc_RuntimeError,
                "Invalid datatable: column %ld has type DT_AUTO", icol);
            return 0;
        }
        if (col.type <= 0 || col.type >= DT_COUNT) {
            PyErr_Format(PyExc_RuntimeError,
                "Invalid datatable: column %ld has unknown type %d",
                icol, col.type);
            return 0;
        }
        if (col.data == NULL && dt->src == NULL) {
            PyErr_Format(PyExc_RuntimeError,
                "Invalid datatable: column %ld has no data, while the "
                "datatable does not have a parent", icol);
            return 0;
        }
        if (col.data == NULL && (col.index < 0 ||
                                 col.index >= dt->src->ncols)) {
            PyErr_Format(PyExc_RuntimeError,
                "Invalid view: column %ld references non-existing column "
                "%ld in the parent datatable", icol, col.index);
            return 0;
        }
        if (col.data == NULL && col.type != dt->src->columns[col.index].type) {
            PyErr_Format(PyExc_RuntimeError,
                "Invalid view: column %ld of type %d references column "
                "%ld of type %d",
                icol, col.type, col.index, dt->src->columns[col.index].type);
            return 0;
        }
    }
    return 1;
}


/**
 * Destructor
 */
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
