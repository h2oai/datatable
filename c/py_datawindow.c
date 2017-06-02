#include <Python.h>
#include "datatable.h"
#include "py_utils.h"
#include "structmember.h"
#include "rowmapping.h"
#include "py_datawindow.h"
#include "py_datatable.h"
#include "py_types.h"

// Forward declarations
static int _check_consistency(DataTable *dt, int64_t row0, int64_t row1,
                              int64_t col0, int64_t col1);
static int _init_hexview(
    DataWindow_PyObject *self, DataTable *dt, int64_t column,
    int64_t row0, int64_t row1, int64_t col0, int64_t col1);

static PyObject* hexcodes[257];

// Module initialization
int init_py_datawindow(PyObject *module)
{
    DataWindow_PyType.tp_new = PyType_GenericNew;
    if (PyType_Ready(&DataWindow_PyType) < 0) return 0;
    Py_INCREF(&DataWindow_PyType);
    PyModule_AddObject(module, "DataWindow", (PyObject*) &DataWindow_PyType);

    for (int i = 0; i < 256; i++) {
        int8_t d0 = i & 0x0F;
        int8_t d1 = (i >> 4) & 0x0F;
        char buf[2];
        buf[0] = d1 < 10? ('0' + d1) : ('A' + (d1 - 10));
        buf[1] = d0 < 10? ('0' + d0) : ('A' + (d0 - 10));
        hexcodes[i] = PyUnicode_FromStringAndSize(buf, 2);
    }
    hexcodes[256] = PyUnicode_FromStringAndSize("  ", 2);
    return 1;
}


/**
 * DataWindow object constructor. This constructor takes a datatable, and
 * coordinates of a data window, and extracts the data from the datatable
 * within the provided window as a pythonic list of lists.
 *
 * :param dt
 *     The datatable whose data window we want to extract.
 *
 * :param row0, row1
 *     Index of the first / last+1 row of the data window.
 *
 * :param col0, col1
 *     Index of the first / last+1 column of the data window.
 *
 * :param column (optional)
 *     If specified, then a "hex view" data window will be returned instead of
 *     the regular data window. The `column` parameter specifies the index of
 *     the column (within the datatable `dt`) whose binary data is accessed. The
 *     rows of the returned datawindow correspond to 16-byte chunks of the
 *     column's binary data. There will be 17 columns in the returned data
 *     window, first 16 are hex representations of each byte within the 16-byte
 *     chunk, and the last column is ASCII rendering of the same chunk.
 */
static int _init_(DataWindow_PyObject *self, PyObject *args, PyObject *kwds)
{
    DataTable_PyObject *pydt;
    DataTable *dt;
    PyObject *stypes = NULL, *ltypes = NULL, *view = NULL;
    int n_init_cols = 0;
    int64_t row0, row1, col0, col1;
    int64_t column = -1;

    // Parse arguments and check their validity
    static char *kwlist[] =
        {"dt", "row0", "row1", "col0", "col1", "column", NULL};
    int ret = PyArg_ParseTupleAndKeywords(
        args, kwds, "O!nnnn|n:DataWindow.__init__", kwlist,
        &DataTable_PyType, &pydt, &row0, &row1, &col0, &col1, &column
    );
    if (!ret) return -1;

    dt = pydt == NULL? NULL : pydt->ref;
    if (column >= 0) {
        return _init_hexview(self, dt, column, row0, row1, col0, col1);
    }

    if (!_check_consistency(dt, row0, row1, col0, col1))
        return -1;

    // Window dimensions
    int64_t ncols = col1 - col0;
    int64_t nrows = row1 - row0;

    // Create and fill-in the `stypes` list
    stypes = PyList_New((Py_ssize_t) ncols);
    ltypes = PyList_New((Py_ssize_t) ncols);
    if (stypes == NULL || ltypes == NULL) goto fail;
    for (int64_t i = col0; i < col1; i++) {
        Column *col = dt->columns[i];
        SType stype = col->stype;
        LType ltype = stype_info[stype].ltype;
        PyList_SET_ITEM(ltypes, i - col0, incref(py_ltype_names[ltype]));
        PyList_SET_ITEM(stypes, i - col0, incref(py_stype_names[stype]));
    }

    RowMapping *rindex = dt->rowmapping;
    int no_rindex = (rindex == NULL);
    int rindex_is_arr32 = rindex && rindex->type == RM_ARR32;
    int rindex_is_arr64 = rindex && rindex->type == RM_ARR64;
    int rindex_is_slice = rindex && rindex->type == RM_SLICE;
    int32_t *rindexarr32 = rindex_is_arr32? rindex->ind32 : NULL;
    int64_t *rindexarr64 = rindex_is_arr64? rindex->ind64 : NULL;
    int64_t rindexstart = rindex_is_slice? rindex->slice.start : 0;
    int64_t rindexstep = rindex_is_slice? rindex->slice.step : 0;

    // Create and fill-in the `data` list
    view = PyList_New((Py_ssize_t) ncols);
    if (view == NULL) goto fail;
    for (int64_t i = col0; i < col1; ++i) {
        Column *col = dt->columns[i];

        PyObject *py_coldata = PyList_New((Py_ssize_t) nrows);
        if (py_coldata == NULL) goto fail;
        PyList_SET_ITEM(view, n_init_cols++, py_coldata);

        int n_init_rows = 0;
        for (int64_t j = row0; j < row1; ++j) {
            int64_t irow = no_rindex? j :
                           rindex_is_arr32? rindexarr32[j] :
                           rindex_is_arr64? (int64_t) rindexarr64[j] :
                                            rindexstart + rindexstep * j;
            PyObject *value = py_stype_formatters[col->stype](col, irow);
            if (value == NULL) goto fail;
            PyList_SET_ITEM(py_coldata, n_init_rows++, value);
        }
    }

    self->row0 = row0;
    self->row1 = row1;
    self->col0 = col0;
    self->col1 = col1;
    self->types = (PyListObject*) ltypes;
    self->stypes = (PyListObject*) stypes;
    self->data = (PyListObject*) view;
    return 0;

  fail:
    Py_XDECREF(stypes);
    Py_XDECREF(ltypes);
    Py_XDECREF(view);
    return -1;
}



static int _init_hexview(
    DataWindow_PyObject *self, DataTable *dt, int64_t colidx,
    int64_t row0, int64_t row1, int64_t col0, int64_t col1)
{
    PyObject *viewdata = NULL;
    PyObject *ltypes = NULL;
    PyObject *stypes = NULL;
    // printf("_init_hexview(%p, %d)\n", dt, colidx);

    if (colidx < 0 || colidx >= dt->ncols) {
        PyErr_Format(PyExc_ValueError, "Invalid column index %lld", colidx);
        return -1;
    }
    Column *column = dt->columns[colidx];

    int64_t maxrows = ((int64_t) column->alloc_size + 15) >> 4;
    if (col0 < 0 || col1 < col0 || col1 > 17 ||
        row0 < 0 || row1 < row0 || row1 > maxrows) {
        PyErr_Format(PyExc_ValueError,
            "Invalid data window bounds: [%ld..%ld x %ld..%ld] "
            "for a column with allocation size %zd",
            row0, row1, col0, col1, column->alloc_size);
        return -1;
    }
    // Window dimensions
    int64_t ncols = col1 - col0;
    int64_t nrows = row1 - row0;
    // printf("ncols = %ld, nrows = %ld\n", ncols, nrows);

    // Create and fill-in the `stypes`/`ltypes` lists
    stypes = TRY(PyList_New(ncols));
    ltypes = TRY(PyList_New(ncols));
    for (int64_t i = 0; i < ncols; i++) {
        PyList_SET_ITEM(ltypes, i, incref(py_ltype_names[LT_STRING]));
        PyList_SET_ITEM(stypes, i, incref(py_stype_names[ST_STRING_FCHAR]));
    }

    uint8_t *coldata = (uint8_t*)(column->data + 16 * row0);
    uint8_t *coldata_end = (uint8_t*)(column->data + column->alloc_size);
    // printf("coldata = %p, end = %p\n", coldata, coldata_end);
    viewdata = TRY(PyList_New(ncols));
    for (int i = 0; i < ncols; i++) {
        PyObject *py_coldata = TRY(PyList_New(nrows));
        PyList_SET_ITEM(viewdata, i, py_coldata);

        if (i < 16) {
            for (int j = 0; j < nrows; j++) {
                uint8_t *ch = coldata + i + (j * 16);
                PyList_SET_ITEM(py_coldata, j,
                    incref(ch < coldata_end? hexcodes[*ch] : hexcodes[256]));
            }
        }
        if (i == 16) {
            for (int j = 0; j < nrows; j++) {
                char buf[16];
                for (int k = 0; k < 16; k++) {
                    uint8_t *ch = coldata + k + (j * 16);
                    buf[k] = ch >= coldata_end? ' ' :
                             (*ch < 0x20 ||( *ch >= 0x7F && *ch < 0xA0))? '.' :
                             (char) *ch;
                }
                PyObject *str =
                    TRY(PyUnicode_Decode(buf, 16, "Latin1", "strict"));
                PyList_SET_ITEM(py_coldata, j, str);
            }
        }
    }

    self->row0 = row0;
    self->row1 = row1;
    self->col0 = col0;
    self->col1 = col1;
    self->data = (PyListObject*) viewdata;
    self->types = (PyListObject*) ltypes;
    self->stypes = (PyListObject*) stypes;
    return 0;

  fail:
    Py_XDECREF(viewdata);
    Py_XDECREF(stypes);
    Py_XDECREF(ltypes);
    return -1;
}



//==============================================================================

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

    // verify that the row index (if present) is valid
    RowMapping *rindex = dt->rowmapping;
    if (rindex != NULL) {
        if (rindex->length != dt->nrows) {
            PyErr_Format(PyExc_RuntimeError,
                "Invalid view: row index has %lld elements, while the "
                "view itself has .nrows = %lld",
                rindex->length, dt->nrows);
            return 0;
        }
        switch (rindex->type) {
            case RM_ARR32: {
                for (int64_t j = row0; j < row1; ++j) {
                    int32_t jsrc = rindex->ind32[j];
                    if (jsrc < 0) {
                        PyErr_Format(PyExc_RuntimeError,
                            "Invalid row %ld in the rowmapping", j);
                        return 0;
                    }
                }
            }   break;

            case RM_ARR64: {
                for (int64_t j = row0; j < row1; ++j) {
                    int64_t jsrc = rindex->ind64[j];
                    if (jsrc < 0) {
                        PyErr_Format(PyExc_RuntimeError,
                            "Invalid row %ld in the rowmapping", j);
                        return 0;
                    }
                }
            }   break;

            case RM_SLICE: {
                int64_t start = rindex->slice.start;
                int64_t count = rindex->length;
                int64_t finish = start + (count - 1) * rindex->slice.step;
                if (start < 0) {
                    PyErr_Format(PyExc_RuntimeError,
                        "Invalid view: first row is %ld", start);
                    return 0;
                }
                if (finish < 0) {
                    PyErr_Format(PyExc_RuntimeError,
                        "Invalid view: last row is %ld", finish);
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
        Column *col = dt->columns[i];
        if (col->stype < 1 || col->stype >= DT_STYPES_COUNT) {
            PyErr_Format(PyExc_RuntimeError,
                "Invalid datatable: column %ld has unknown type %d",
                i, col->stype);
            return 0;
        }
        if (col->meta == NULL && stype_info[col->stype].metasize > 0) {
            PyErr_Format(PyExc_RuntimeError,
                "Invalid datatable: column %ld has type %s but meta info is "
                "missing",
                i, stype_info[col->stype].code);
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
    Py_XDECREF(self->stypes);
    Py_TYPE(self)->tp_free((PyObject*)self);
}


//------ Declare the DataWindow object -----------------------------------------

PyDoc_STRVAR(dtdoc_datawindow, "DataWindow object");
PyDoc_STRVAR(dtdoc_row0, "Index of the first row");
PyDoc_STRVAR(dtdoc_row1, "Index of the last row exclusive");
PyDoc_STRVAR(dtdoc_col0, "Index of the first column");
PyDoc_STRVAR(dtdoc_col1, "Index of the last column exclusive");
PyDoc_STRVAR(dtdoc_types, "Types of the columns within the view");
PyDoc_STRVAR(dtdoc_stypes, "Storage types of the columns within the view");
PyDoc_STRVAR(dtdoc_data, "Datatable's data within the specified window");


#define Member(name, type) {#name, type, offsetof(DataWindow_PyObject, name), \
                            READONLY, dtdoc_ ## name}

static PyMemberDef members[] = {
    Member(row0, T_LONG),
    Member(row1, T_LONG),
    Member(col0, T_LONG),
    Member(col1, T_LONG),
    Member(types, T_OBJECT_EX),
    Member(stypes, T_OBJECT_EX),
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
    (initproc)_init_,                   /* tp_init */
    0,0,0,0,0,0,0,0,0,0,0,0
};
