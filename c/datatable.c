#include "Python.h"
#include "structmember.h"
#include "limits.h"
#include "datatable.h"
#include "dtutils.h"
#include "rows.h"
#include "datawindow.h"


static int _fill_1_column(PyObject *list, dt_Column *column);
static int _allocate_column(dt_Column *column, long nrows);
static int _switch_to_coltype(dt_Coltype newtype, PyObject *list, dt_Column *column);
// static dt_DatatableObject* _preallocate_Datatable(long nrows, int ncols, dt_Coltype* types);


static dt_DatatableObject* omni(dt_DatatableObject *self, PyObject* args)
{
    dt_Column *columns = NULL;

    dt_RowsIndexObject *rows;
    if (!PyArg_ParseTuple(args, "O!:omni", &dt_RowsIndexType, &rows))
        return NULL;

    int ncols = self->ncols;
    long nrows = rows->kind == RI_ARRAY? rows->riArray.length :
                 rows->kind == RI_SLICE? rows->riSlice.count : 0;

    dt_DatatableObject* res = (dt_DatatableObject*)
        PyObject_CallObject((PyObject*) &dt_DatatableType, NULL);
    if (res == NULL) goto fail;

    columns = malloc(sizeof(dt_Column) * ncols);
    if (columns == NULL) goto fail;
    for (int i = 0; i < ncols; ++i) {
        columns[i].data = NULL;
        columns[i].index = i;
        columns[i].type = self->columns[i].type;
        columns[i].stats = NULL;
    }

    res->ncols = ncols;
    res->nrows = nrows;
    res->src = self->src == NULL? self : self->src;
    res->row_index = rows;
    res->columns = columns;
    Py_XINCREF(self);
    Py_XINCREF(rows);
    return res;

fail:
    Py_XDECREF(res);
    free(columns);
    return NULL;
}

/*
static dt_DatatableObject* omni(dt_DatatableObject *self, PyObject* args) {

    dt_RowsIndexObject *rows;
    if (!PyArg_ParseTuple(args, "O!:omni", &dt_RowsIndexType, &rows))
        return NULL;

    int ncols = self->ncols;
    long nrows = rows->kind == RI_ARRAY? rows->riArray.length :
                 rows->kind == RI_SLICE? rows->riSlice.count : 0;
    dt_Coltype* types = malloc(ncols * sizeof(dt_Coltype));
    if (types == NULL) return NULL;
    for (int i = 0; i < ncols; ++i)
        types[i] = self->columns[i].type;

    dt_DatatableObject *res = _preallocate_Datatable(nrows, ncols, types);
    if (res == NULL) return NULL;

    // For now we can only filter rows. So do just that
    switch (rows->kind) {
        case RI_ARRAY: {
            long *indices = rows->riArray.rows;
            for (int j = 0; j < ncols; ++j) {
                void *srccol = self->columns[j].data;
                void *trgcol = res->columns[j].data;
                int elemsize = dt_Coltype_size[types[j]];
                for (long i = 0; i < nrows; ++i) {
                    memcpy(trgcol + i*elemsize, srccol + indices[i]*elemsize, elemsize);
                }
            }
        }   break;

        case RI_SLICE: {
            long start = rows->riSlice.start;
            long count = rows->riSlice.count;
            long step = rows->riSlice.step;

            if (step == 1) {
                // Slice is a contiguous memory range -- use memcpy
                for (int j = 0; j < ncols; ++j) {
                    void *srccol = self->columns[j].data;
                    void *trgcol = res->columns[j].data;
                    int elemsize = dt_Coltype_size[types[j]];
                    memcpy(trgcol, srccol + start*elemsize, count*elemsize);
                }
            } else {
                for (int j = 0; j < ncols; ++j) {
                    void *srccol = self->columns[j].data;
                    void *trgcol = res->columns[j].data;
                    int elemsize = dt_Coltype_size[types[j]];
                    for (long i = 0, row = start; i < count; ++i, row += step) {
                        memcpy(trgcol + i*elemsize, srccol + row*elemsize, elemsize);
                    }
                }
            }
        }   break;

        default:
            assert(0);
    }

    for (int j = 0; j < ncols; ++j) {
        if (types[j] == DT_OBJECT) {
            PyObject* ptr = (PyObject*) (res->columns + j);
            for (long i = 0; i < nrows; ++i) {
                Py_XINCREF(ptr + i);
            }
        }
    }
    free(types);
    return res;
}

static dt_DatatableObject* _preallocate_Datatable(long nrows, int ncols, dt_Coltype *types)
{
    int n_columns_allocated = 0;
    PyTypeObject* dttype = &dt_DatatableType;

    dt_DatatableObject* res = (dt_DatatableObject*) dttype->tp_alloc(dttype, 0);
    if (res == NULL) goto fail;

    dt_Column *columns = clone(NULL, sizeof(dt_Column*) * ncols);
    if (columns == NULL) goto fail;

    for (int i = 0; i < ncols; i++) {
        int ret = _allocate_column(columns + i, nrows);
        if (ret == -1) goto fail;
        n_columns_allocated++;
    }

    res->ncols = ncols;
    res->nrows = nrows;
    res->columns = columns;
    return res;

fail:
    for (int i = 0; i < n_columns_allocated; i++) {
        free(columns[i].data);
    }
    free(columns);
    Py_XDECREF(res);
    return NULL;
}
*/


//======================================================================================================================
// Make Datatable from a Python list
//======================================================================================================================


/**
 * Exported class method to instantiate a new Datatable object from a list.
 *
 * If the list is empty, then an empty (0 x 0) datatable is produced.
 * If the list is a list of lists, then inner lists are assumed to be the
 * columns, then the number of elements in these lists must be the same
 * and it will be the number of rows in the datatable produced.
 * Otherwise, we assume that the list represents a single data column, and
 * build the datatable appropriately.
 *
 * @param list: A list, or list of lists.
 */
static PyObject* dt_Datatable_fromlist(PyTypeObject *type, PyObject *args)
{
    PyObject *list;
    if (!PyArg_ParseTuple(args, "O!:from_list", &PyList_Type, &list))
        return NULL;

    // Create a new (empty) DataTable instance
    dt_DatatableObject* self = (dt_DatatableObject*)
        PyObject_CallObject((PyObject*) &dt_DatatableType, NULL);
    if (self == NULL) goto fail;
    assert(self->src == NULL && self->row_index == NULL && self->columns == NULL);

    // if the supplied list is empty, return the empty Datatable object
    Py_ssize_t listsize = Py_SIZE(list);  // works both for lists and tuples
    if (listsize == 0) {
        self->ncols = 0;
        self->nrows = 0;
        return (PyObject*) self;
    }

    PyObject *item0 = PyList_GET_ITEM(list, 0);
    int item0_is_list = PyList_Check(item0);
    if (item0_is_list) {
        // List-of-lists case: create datatable with as many columns as
        // there are elements in the outer list.
        Py_ssize_t item0size = Py_SIZE(item0);

        if (listsize > INT_MAX) {
            PyErr_SetString(PyExc_ValueError, "Too many columns for the datatable");
            return NULL;
        }

        // Basic check validity of the provided data.
        for (int i = 1; i < listsize; ++i) {
            PyObject *item = PyList_GET_ITEM(list, i);
            if (!PyList_Check(item)) {
                PyErr_SetString(PyExc_ValueError, "Source list contains both lists and non-lists");
                return NULL;
            }
            if (Py_SIZE(item) != item0size) {
                PyErr_SetString(PyExc_ValueError, "Source lists have varying number of rows");
                return NULL;
            }
        }

        self->ncols = (int) listsize;
        self->nrows = (long) item0size;
    } else {
        // Single column case
        self->ncols = 1;
        self->nrows = (long) listsize;
    }

    // Allocate memory for the datatable's columns
    int ncols = self->ncols;
    self->columns = malloc(sizeof(dt_Column) * ncols);
    if (self->columns == NULL) goto fail;
    for (int i = 0; i < ncols; ++i) {
        self->columns[i].type = DT_AUTO;
        self->columns[i].data = NULL;
        self->columns[i].index = -1;
        self->columns[i].stats = NULL;
    }

    // Fill the data
    for (int i = 0; i < ncols; ++i) {
        self->columns[i].type = DT_AUTO;
        PyObject *src = item0_is_list? PyList_GET_ITEM(list, i) : list;
        int ret = _fill_1_column(src, self->columns + i);
        if (ret == -1) goto fail;
    }

    return (PyObject*) self;

fail:
    Py_XDECREF(self);
    return NULL;
}


/**
 * Create a single column of data from the python list.
 *
 * @param list: the data source.
 * @param coltype: the desired dtype for the column; if DT_AUTO then this value
 *     will be modified in-place to an appropriate type.
 * @param coldata: pointer to a ``dt_Coltype`` structure that will be modified
 *     by reference to fill in the column's data.
 * @returns 0 on success, -1 on error
 */
static int _fill_1_column(PyObject *list, dt_Column *column) {
    long nrows = (long) Py_SIZE(list);
    if (nrows == 0) {
        column->data = NULL;
        column->type = DT_DOUBLE;
        return 0;
    }

    int ret = _allocate_column(column, nrows);
    if (ret == -1) return -1;

    int overflow;
    void *col = column->data;
    for (long i = 0; i < nrows; ++i) {
        PyObject *item = PyList_GET_ITEM(list, i);  // borrowed ref
        PyTypeObject *itemtype = Py_TYPE(item);     // borrowed ref

        if (item == Py_None) {
            //---- store NaN value ----
            switch (column->type) {
                case DT_DOUBLE: ((double*)col)[i] = NAN; break;
                case DT_LONG:   ((long*)col)[i] = LONG_MIN; break;
                case DT_BOOL:   ((char*)col)[i] = 2; break;
                case DT_STRING: ((char**)col)[i] = NULL; break;
                case DT_OBJECT: ((PyObject**)col)[i] = incref(item); break;
                case DT_AUTO:   /* do nothing */ break;
            }

        } else if (itemtype == &PyLong_Type) {
            //---- store an integer ----
            long_case:
            switch (column->type) {
                case DT_LONG: {
                    long val = PyLong_AsLongAndOverflow(item, &overflow);
                    if (overflow)
                        return _switch_to_coltype(DT_DOUBLE, list, column);
                    ((long*)col)[i] = val;
                }   break;

                case DT_DOUBLE:
                    ((double*)col)[i] = PyLong_AsDouble(item);
                    break;

                case DT_BOOL: {
                    long val = PyLong_AsLongAndOverflow(item, &overflow);
                    if (overflow || (val != 0 && val != 1))
                        return _switch_to_coltype(overflow? DT_DOUBLE : DT_LONG, list, column);
                    ((char*)col)[i] = (unsigned char) val;
                } break;

                case DT_STRING:
                    // not supported yet
                    return _switch_to_coltype(DT_OBJECT, list, column);

                case DT_OBJECT:
                    ((PyObject**)col)[i] = incref(item);
                    break;

                case DT_AUTO: {
                    long val = PyLong_AsLongAndOverflow(item, &overflow);
                    return _switch_to_coltype(
                        (val == 0 || val == 1) && !overflow? DT_BOOL :
                        overflow? DT_DOUBLE : DT_LONG, list, column
                    );
                }
            }

        } else if (itemtype == &PyFloat_Type) {
            //---- store a real number ----
            float_case: {}
            double val = PyFloat_AS_DOUBLE(item);
            switch (column->type) {
                case DT_DOUBLE:
                    ((double*)col)[i] = val;
                    break;

                case DT_LONG: {
                    double intpart, fracpart = modf(val, &intpart);
                    if (fracpart != 0 || intpart <= LONG_MIN || intpart >LONG_MAX)
                        return _switch_to_coltype(DT_DOUBLE, list, column);
                    ((long*)col)[i] = (long) intpart;
                }   break;

                case DT_BOOL: {
                    if (val != 0 && val != 1)
                        return _switch_to_coltype(DT_DOUBLE, list, column);
                    ((char*)col)[i] = (char) (val == 1);
                }   break;

                case DT_STRING:
                    // not supported yet
                    return _switch_to_coltype(DT_OBJECT, list, column);

                case DT_OBJECT:
                    ((PyObject**)col)[i] = incref(item);
                    break;

                case DT_AUTO: {
                    double intpart, fracpart = modf(val, &intpart);
                    return _switch_to_coltype(
                        val == 0 || val == 1? DT_BOOL :
                        fracpart == 0 && (LONG_MIN < intpart && intpart < LONG_MAX)? DT_LONG : DT_DOUBLE,
                        list, column
                    );
                }
            }

        } else if (itemtype == &PyBool_Type) {
            unsigned char val = (item == Py_True);
            switch (column->type) {
                case DT_BOOL:   ((char*)col)[i] = val;  break;
                case DT_LONG:   ((long*)col)[i] = (long) val;  break;
                case DT_DOUBLE: ((double*)col)[i] = (double) val;  break;
                case DT_STRING: ((char**)col)[i] = val? strdup("1") : strdup("0"); break;
                case DT_OBJECT: ((PyObject**)col)[i] = incref(item);  break;
                case DT_AUTO:   return _switch_to_coltype(DT_BOOL, list, column);
            }

        } else if (itemtype == &PyUnicode_Type) {
            return _switch_to_coltype(DT_OBJECT, list, column);

        } else {
            if (column->type == DT_OBJECT) {
                ((PyObject**)col)[i] = incref(item);
            } else {
                // These checks will be true only if someone subclassed base
                // types, in which case we still want to treat them as
                // primitives but avoid more expensive Py*_Check in the
                // common case.
                if (PyLong_Check(item)) goto long_case;
                if (PyFloat_Check(item)) goto float_case;

                return _switch_to_coltype(DT_OBJECT, list, column);
            }
        }
    }

    // If all values in the column were NaNs, then cast that column as DT_DOUBLE
    if (column->type == DT_AUTO) {
        return _switch_to_coltype(DT_DOUBLE, list, column);
    }
    return 0;
}



/**
 * Allocate memory in ``coldata`` for ``nrows`` elements of type ``coltype``.
 * @returns 0 on success, -1 on error
 */
static int _allocate_column(dt_Column *column, long nrows) {
    column->data = malloc(dt_Coltype_size[column->type] * nrows);
    return column->data == NULL? -1 : 0;
}



/**
 * Switch to a different column type and then re-run `_fill_1_column()`.
 */
static int _switch_to_coltype(dt_Coltype newtype, PyObject *list, dt_Column *column) {
    free(column->data);
    column->type = newtype;
    return _fill_1_column(list, column);
}



//======================================================================================================================
// Other Datatable methods
//======================================================================================================================

static void dt_Datatable_dealloc(dt_DatatableObject *self)
{
    if (self->ncols) {
        for (int i = 0; i < self->ncols; ++i) {
            dt_Column column = self->columns[i];
            if (column.type == DT_OBJECT) {
                int j = self->nrows;
                while (--j >= 0) {
                    Py_XDECREF(((PyObject**) column.data)[j]);
                }
            }
            free(column.data);
            // free(column.stats);
        }
    }
    free(self->columns);
    Py_XDECREF(self->src);
    Py_XDECREF(self->row_index);
    Py_TYPE(self)->tp_free((PyObject*)self);
}


static PyObject* dt_Datatable_view(dt_DatatableObject *self, PyObject *args)
{
    long col0, ncols, row0, nrows;
    if (!PyArg_ParseTuple(args, "llll", &col0, &ncols, &row0, &nrows))
        return NULL;

    PyObject *nargs = Py_BuildValue("Ollll", self, col0, ncols, row0, nrows);
    PyObject *res = PyObject_CallObject((PyObject*) &dt_DataWindowType, nargs);
    Py_XDECREF(nargs);

    return res;
}


PyDoc_STRVAR(dtdoc_ncols, "Number of columns");
PyDoc_STRVAR(dtdoc_nrows, "Number of rows");
PyDoc_STRVAR(dtdoc_view, "Retrieve datatable's data within a window");
PyDoc_STRVAR(dtdoc_fromlist, "Create Datatable from a list");
PyDoc_STRVAR(dtdoc_omni, "Main function for datatable transformation");
PyDoc_STRVAR(dtdoc_src, "Source datatable for a view");
PyDoc_STRVAR(dtdoc_row_index, "Row index (within the source datatable) for a view");

static PyMethodDef dt_Datatable_methods[] = {
    {"window", (PyCFunction)dt_Datatable_view, METH_VARARGS, dtdoc_view},
    {"from_list", (PyCFunction)dt_Datatable_fromlist, METH_VARARGS | METH_CLASS, dtdoc_fromlist},
    {"omni", (PyCFunction)omni, METH_VARARGS, dtdoc_omni},
    {NULL, NULL}           /* sentinel */
};


#define Member(name, type) {#name, type, offsetof(dt_DatatableObject, name), \
                            READONLY, dtdoc_ ## name}

static PyMemberDef dt_Datatable_members[] = {
    Member(ncols, T_INT),
    Member(nrows, T_LONG),
    Member(src, T_OBJECT),
    Member(row_index, T_OBJECT),
    {NULL}                 /* sentinel */
};


PyTypeObject dt_DatatableType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "_datatable.DataTable",             /* tp_name */
    sizeof(dt_DatatableObject),         /* tp_basicsize */
    0,                                  /* tp_itemsize */
    (destructor)dt_Datatable_dealloc,   /* tp_dealloc */
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
    dt_Datatable_methods,               /* tp_methods */
    dt_Datatable_members,               /* tp_members */
    0,                                  /* tp_getset */
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
