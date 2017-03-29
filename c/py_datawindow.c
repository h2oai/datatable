#include <Python.h>
#include "datatable.h"
#include "dtutils.h"
#include "structmember.h"
#include "rowmapping.h"
#include "py_datawindow.h"
#include "py_datatable.h"

// Forward declarations
static int _check_consistency(DataTable *dt, int64_t row0, int64_t row1,
                              int64_t col0, int64_t col1);

int init_py_datawindow(PyObject *module)
{
    DataWindow_PyType.tp_new = PyType_GenericNew;
    if (PyType_Ready(&DataWindow_PyType) < 0) return 0;
    Py_INCREF(&DataWindow_PyType);
    PyModule_AddObject(module, "DataWindow", (PyObject*) &DataWindow_PyType);
    return 1;
}


/**
 * DataWindow object constructor. This constructor takes a datatable, and
 * coordinates of a data window, and extracts the data from the datatable
 * within the provided window as a pythonic list of lists.
 *
 * :param dt: the datatable whose data window we want to extract.
 * :param row0: index of the first row of the data window.
 * :param row1: index of the last + 1 row of the data window.
 * :param col0: index of the first column of the data window.
 * :param col1: index of the last + 1 column of the data window.
 */
static int __init__(DataWindow_PyObject *self, PyObject *args, PyObject *kwds)
{
    DataTable_PyObject *pydt;
    DataTable *dt;
    PyObject *types = NULL, *view = NULL;
    int n_init_types = 0;
    int n_init_cols = 0;
    int64_t row0, row1, col0, col1;

    // Parse arguments and check their validity
    static char *kwlist[] = {"dt", "row0", "row1", "col0", "col1", NULL};
    int ret = PyArg_ParseTupleAndKeywords(args, kwds, "O!llll:DataWindow.__init__",
                                          kwlist, &DataTable_PyType, &pydt,
                                          &row0, &row1, &col0, &col1);
    dt = pydt == NULL? NULL : pydt->ref;
    if (!ret || !_check_consistency(dt, row0, row1, col0, col1))
        return -1;

    // Window dimensions
    int64_t ncols = col1 - col0;
    int64_t nrows = row1 - row0;

    // Create and fill-in the `types` list
    types = PyList_New((Py_ssize_t) ncols);
    if (types == NULL) goto fail;
    for (int64_t i = col0; i < col1; ++i) {
        Column column = dt->columns[i];
        PyObject *py_coltype = PyLong_FromLong(column.type);
        if (py_coltype == NULL) goto fail;
        PyList_SET_ITEM(types, n_init_types++, py_coltype);
    }

    RowMapping *rindex = dt->rowmapping;
    int rindex_is_array = rindex && rindex->type == RI_ARRAY;
    int rindex_is_slice = rindex && rindex->type == RI_SLICE;
    int64_t *rindexarray = rindex_is_array? rindex->indices : NULL;
    int64_t rindexstart = rindex_is_slice? rindex->slice.start : 0;
    int64_t rindexstep = rindex_is_slice? rindex->slice.step : 0;

    // Create and fill-in the `data` list
    view = PyList_New((Py_ssize_t) ncols);
    if (view == NULL) goto fail;
    for (int64_t i = col0; i < col1; ++i) {
        Column column = dt->columns[i];
        int realdata = (column.data != NULL);
        void *coldata = realdata? column.data
                                : dt->source->columns[column.srcindex].data;

        PyObject *py_coldata = PyList_New((Py_ssize_t) nrows);
        if (py_coldata == NULL) goto fail;
        PyList_SET_ITEM(view, n_init_cols++, py_coldata);

        int n_init_rows = 0;
        for (int64_t j = row0; j < row1; ++j) {
            int64_t irow = realdata? j :
                           rindex_is_array? rindexarray[j] :
                                            rindexstart + rindexstep * j;
            PyObject *value = NULL;
            switch (column.type) {
                case DT_DOUBLE: {
                    double x = ((double*)coldata)[irow];
                    value = isnan(x)? none() : PyFloat_FromDouble(x);
                }   break;

                case DT_LONG: {
                    int64_t x = ((int64_t*)coldata)[irow];
                    value = x == LONG_MIN? none() : PyLong_FromLongLong(x);
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
                    Py_XINCREF(value);
                }   break;

                case DT_AUTO:
                default:
                    assert(0);
            }
            if (value == NULL) {
                while (n_init_rows < nrows)
                    PyList_SET_ITEM(py_coldata, n_init_rows++, NULL);
                goto fail;
            }
            PyList_SET_ITEM(py_coldata, n_init_rows++, value);
        }
    }

    self->row0 = row0;
    self->row1 = row1;
    self->col0 = col0;
    self->col1 = col1;
    self->types = (PyListObject*) types;
    self->data = (PyListObject*) view;
    return 0;

  fail:
    Py_XDECREF(types);
    Py_XDECREF(view);
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
 * :param row0: index of the first row of the data window.
 * :param row1: index of the last + 1 row of the data window.
 * :param col0: index of the first column of the data window.
 * :param col1: index of the last + 1 column of the data window.
 * :returns: 1 on success, 0 on failure
 */
static int _check_consistency(
    DataTable *dt, int64_t row0, int64_t row1, int64_t col0, int64_t col1)
{
    // check correctness of the data window
    if (col0 < 0 || col1 < col0 || col1 > dt->ncols ||
        row0 < 0 || row1 < row0 || row1 > dt->nrows) {
        PyErr_Format(PyExc_ValueError,
            "Invalid data window bounds: datatable is [%ld x %ld], "
            "whereas requested window is [%ld..%ld x %ld..%ld]",
            dt->nrows, dt->ncols, row0, row1, col0, col1);
        return 0;
    }

    // verify that the datatable is internally consistent
    RowMapping *rindex = dt->rowmapping;
    if (rindex == NULL && dt->source != NULL) {
        PyErr_SetString(PyExc_RuntimeError,
            "Invalid datatable: .source is present, but .rowmapping is null");
        return 0;
    }
    if (rindex != NULL && dt->source == NULL) {
        PyErr_SetString(PyExc_RuntimeError,
            "Invalid datatable: .source is null, while .rowmapping is present");
        return 0;
    }
    if (dt->source != NULL && dt->source->source != NULL) {
        PyErr_SetString(PyExc_RuntimeError,
            "Invalid view: must not have another view as a parent");
        return 0;
    }

    // verify that the row index (if present) is valid
    if (rindex != NULL) {
        switch (rindex->type) {
            case RI_ARRAY: {
                if (rindex->length != dt->nrows) {
                    PyErr_Format(PyExc_RuntimeError,
                        "Invalid view: row index has %ld elements, while the "
                        "view itself has .nrows = %ld",
                        rindex->length, dt->nrows);
                    return 0;
                }
                for (int64_t j = row0; j < row1; ++j) {
                    int64_t jsrc = rindex->indices[j];
                    if (jsrc < 0 || jsrc >= dt->source->nrows) {
                        PyErr_Format(PyExc_RuntimeError,
                            "Invalid view: row %ld of the view references non-"
                            "existing row %ld in the source datatable",
                            j, jsrc);
                        return 0;
                    }
                }
            }   break;

            case RI_SLICE: {
                int64_t start = rindex->slice.start;
                int64_t count = rindex->length;
                int64_t finish = start + (count - 1) * rindex->slice.step;
                if (count != dt->nrows) {
                    PyErr_Format(PyExc_RuntimeError,
                        "Invalid view: row index has %ld elements, while the "
                        "view itself has .nrows = %ld", count, dt->nrows);
                    return 0;
                }
                if (start < 0 || start >= dt->source->nrows) {
                    PyErr_Format(PyExc_RuntimeError,
                        "Invalid view: first row references an invalid row "
                        "%ld in the parent datatable", start);
                    return 0;
                }
                if (finish < 0 || finish >= dt->source->nrows) {
                    PyErr_Format(PyExc_RuntimeError,
                        "Invalid view: last row references an invalid row "
                        "%ld in the parent datatable", finish);
                    return 0;
                }
            }   break;

            default:
                PyErr_Format(PyExc_RuntimeError,
                    "Unexpected row index of type = %d", rindex->type);
                return 0;
        }
    }

    // check each column within the window for correctness
    for (int64_t i = col0; i < col1; ++i) {
        Column col = dt->columns[i];
        Column *srccols = dt->source == NULL? NULL : dt->source->columns;
        if (col.type == DT_AUTO) {
            PyErr_Format(PyExc_RuntimeError,
                "Invalid datatable: column %ld has type DT_AUTO", i);
            return 0;
        }
        if (col.type <= 0 || col.type >= DT_COUNT) {
            PyErr_Format(PyExc_RuntimeError,
                "Invalid datatable: column %ld has unknown type %d",
                i, col.type);
            return 0;
        }
        if (col.data == NULL && dt->source == NULL) {
            PyErr_Format(PyExc_RuntimeError,
                "Invalid datatable: column %ld has no data, while the "
                "datatable does not have a parent", i);
            return 0;
        }
        if (col.data == NULL && (col.srcindex < 0 ||
                                 col.srcindex >= dt->source->ncols)) {
            PyErr_Format(PyExc_RuntimeError,
                "Invalid view: column %ld references non-existing column "
                "%ld in the parent datatable", i, col.srcindex);
            return 0;
        }
        if (col.data == NULL && col.type != srccols[col.srcindex].type) {
            PyErr_Format(PyExc_RuntimeError,
                "Invalid view: column %ld of type %d references column "
                "%ld of type %d",
                i, col.type, col.srcindex, srccols[col.srcindex].type);
            return 0;
        }
    }
    return 1;
}


/**
 * Destructor
 */
static void __dealloc__(DataWindow_PyObject *self)
{
    Py_XDECREF(self->data);
    Py_XDECREF(self->types);
    Py_TYPE(self)->tp_free((PyObject*)self);
}


//------ Declare the DataWindow object -----------------------------------------

PyDoc_STRVAR(dtdoc_datawindow, "DataWindow object");
PyDoc_STRVAR(dtdoc_row0, "Index of the first row");
PyDoc_STRVAR(dtdoc_row1, "Index of the last row exclusive");
PyDoc_STRVAR(dtdoc_col0, "Index of the first column");
PyDoc_STRVAR(dtdoc_col1, "Index of the last column exclusive");
PyDoc_STRVAR(dtdoc_types, "Types of the columns within the view");
PyDoc_STRVAR(dtdoc_data, "Datatable's data within the specified window");


#define Member(name, type) {#name, type, offsetof(DataWindow_PyObject, name), \
                            READONLY, dtdoc_ ## name}

static PyMemberDef members[] = {
    Member(row0, T_LONG),
    Member(row1, T_LONG),
    Member(col0, T_LONG),
    Member(col1, T_LONG),
    Member(types, T_OBJECT_EX),
    Member(data, T_OBJECT_EX),
    {NULL, 0, 0, 0, NULL}  // sentinel
};


PyTypeObject DataWindow_PyType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "_datatable.DataWindow",            /* tp_name */
    sizeof(DataWindow_PyObject),        /* tp_basicsize */
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
    0,0,0,0,0,0,0,0,0,0,0,0
};
