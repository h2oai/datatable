#include "Python.h"
#include "structmember.h"
#include "limits.h"
#include "datatable.h"
#include "dtutils.h"


//======================================================================================================================
// Make Datatable from a Python list
//======================================================================================================================

static int _fill_1_column(PyObject *list, dt_Coltype *coltype, dt_Coldata *coldata);
static void _make_0rows_column(dt_Coltype *coltype, dt_Coldata *coldata);
static int _allocate_column(dt_Coltype *coltype, dt_Coldata *coldata, long nrows);
static void _deallocate_column(dt_Coltype *coltype, dt_Coldata *coldata);
static int _switch_to_coltype(dt_Coltype newtype, PyObject *list, dt_Coltype *coltype, dt_Coldata *coldata);


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

    // type->tp_alloc() produces a new instance and raises its REFCNT
    dt_DatatableObject *self = (dt_DatatableObject*) type->tp_alloc(type, 0);
    if (self == NULL) return NULL;

    // if the supplied list is empty, return the Datatable object as-is
    Py_ssize_t listsize = Py_SIZE(list);  // works both for lists and tuples
    if (listsize == 0)
        return (PyObject*) self;

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

    // Allocate memory for the datatable
    self->coltypes = (dt_Coltype*) malloc(sizeof(dt_Coltype) * self->ncols);
    self->columns = (dt_Coldata*) malloc(sizeof(dt_Coldata) * self->ncols);
    if (self->coltypes == NULL || self->columns == NULL) return decref((PyObject*) self);

    // Fill the data
    for (int i = 0; i < self->ncols; ++i) {
        self->coltypes[i] = DT_AUTO;
        PyObject *src = item0_is_list? PyList_GET_ITEM(list, i) : list;
        int ret = _fill_1_column(src, self->coltypes + i, self->columns + i);
        if (ret == -1) return decref((PyObject*) self);
    }

    return (PyObject*) self;
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
static int _fill_1_column(PyObject *list, dt_Coltype *coltype, dt_Coldata *coldata) {
    long nrows = (long) Py_SIZE(list);
    if (nrows == 0) {
        _make_0rows_column(coltype, coldata);
        return 0;
    }

    int ret = _allocate_column(coltype, coldata, nrows);
    if (ret == -1) return -1;

    int overflow;
    for (long i = 0; i < nrows; ++i) {
        PyObject *item = PyList_GET_ITEM(list, i);  // borrowed ref
        PyTypeObject *itemtype = Py_TYPE(item);     // borrowed ref

        if (item == Py_None) {
            //---- store NaN value ----
            switch (*coltype) {
                case DT_DOUBLE: coldata->ddouble[i] = NAN; break;
                case DT_LONG:   coldata->dlong[i] = LONG_MIN; break;
                case DT_BOOL:   coldata->dbool[i] = 2; break;
                case DT_STRING: coldata->dstring[i] = NULL; break;
                case DT_OBJECT: coldata->dobject[i] = incref(item); break;
                case DT_AUTO:   /* do nothing */ break;
            }

        } else if (itemtype == &PyLong_Type) {
            //---- store an integer ----
            long_case:
            switch (*coltype) {
                case DT_LONG: {
                    long val = PyLong_AsLongAndOverflow(item, &overflow);
                    if (overflow)
                        return _switch_to_coltype(DT_DOUBLE, list, coltype, coldata);
                    coldata->dlong[i] = val;
                }   break;

                case DT_DOUBLE:
                    coldata->ddouble[i] = PyLong_AsDouble(item);
                    break;

                case DT_BOOL: {
                    long val = PyLong_AsLongAndOverflow(item, &overflow);
                    if (overflow || (val != 0 && val != 1))
                        return _switch_to_coltype(overflow? DT_DOUBLE : DT_LONG, list, coltype, coldata);
                    coldata->dbool[i] = (unsigned char) val;
                } break;

                case DT_STRING:
                    // not supported yet
                    return _switch_to_coltype(DT_OBJECT, list, coltype, coldata);

                case DT_OBJECT:
                    coldata->dobject[i] = incref(item);
                    break;

                case DT_AUTO: {
                    long val = PyLong_AsLongAndOverflow(item, &overflow);
                    return _switch_to_coltype(
                        (val == 0 || val == 1) && !overflow? DT_BOOL :
                        overflow? DT_DOUBLE : DT_LONG,
                        list, coltype, coldata
                    );
                }
            }

        } else if (itemtype == &PyFloat_Type) {
            //---- store a real number ----
            float_case: {}
            double val = PyFloat_AS_DOUBLE(item);
            switch (*coltype) {
                case DT_DOUBLE:
                    coldata->ddouble[i] = val;
                    break;

                case DT_LONG: {
                    double intpart, fracpart = modf(val, &intpart);
                    if (fracpart != 0 || intpart <= LONG_MIN || intpart >LONG_MAX)
                        return _switch_to_coltype(DT_DOUBLE, list, coltype, coldata);
                    coldata->dlong[i] = (long) intpart;
                }   break;

                case DT_BOOL: {
                    if (val != 0 && val != 1)
                        return _switch_to_coltype(DT_DOUBLE, list, coltype, coldata);
                    coldata->dbool[i] = (unsigned char) (val == 1);
                }   break;

                case DT_STRING:
                    // not supported yet
                    return _switch_to_coltype(DT_OBJECT, list, coltype, coldata);

                case DT_OBJECT:
                    coldata->dobject[i] = incref(item);
                    break;

                case DT_AUTO: {
                    double intpart, fracpart = modf(val, &intpart);
                    return _switch_to_coltype(
                        val == 0 || val == 1? DT_BOOL :
                        fracpart == 0 && (LONG_MIN < intpart && intpart < LONG_MAX)? DT_LONG : DT_DOUBLE,
                        list, coltype, coldata
                    );
                }
            }

        } else if (itemtype == &PyBool_Type) {
            unsigned char val = (item == Py_True);
            switch (*coltype) {
                case DT_BOOL:   coldata->dbool[i] = val;  break;
                case DT_LONG:   coldata->dlong[i] = (long) val;  break;
                case DT_DOUBLE: coldata->ddouble[i] = (double) val;  break;
                case DT_STRING: coldata->dstring[i] = val? strdup("1") : strdup("0"); break;
                case DT_OBJECT: coldata->dobject[i] = incref(item);  break;
                case DT_AUTO:   return _switch_to_coltype(DT_BOOL, list, coltype, coldata);
            }

        } else if (itemtype == &PyUnicode_Type) {
            return _switch_to_coltype(DT_OBJECT, list, coltype, coldata);

        } else {
            if (*coltype == DT_OBJECT) {
                coldata->dobject[i] = incref(item);
            } else {
                // These checks will be true only if someone subclassed base
                // types, in which case we still want to treat them as
                // primitives but avoid more expensive Py*_Check in the
                // common case.
                if (PyLong_Check(item)) goto long_case;
                if (PyFloat_Check(item)) goto float_case;

                return _switch_to_coltype(DT_OBJECT, list, coltype, coldata);
            }
        }
    }

    // If all values in the column were NaNs, then cast that column as DT_DOUBLE
    if (*coltype == DT_AUTO) {
        return _switch_to_coltype(DT_DOUBLE, list, coltype, coldata);
    }
    return 0;
}


/**
 * Create a single column of data with 0 rows. This just sets the pointer
 * for the allocated data array to NULL (since creating 0-elements arrays may
 * not be portable in C).
 */
static void _make_0rows_column(dt_Coltype *coltype, dt_Coldata *coldata) {
    switch (*coltype) {
        case DT_AUTO:   *coltype = DT_DOUBLE;  /* fall-through */
        case DT_DOUBLE: coldata->ddouble = NULL;  break;
        case DT_LONG:   coldata->dlong = NULL;    break;
        case DT_BOOL:   coldata->dbool = NULL;    break;
        case DT_STRING: coldata->dstring = NULL;  break;
        case DT_OBJECT: coldata->dobject = NULL;  break;
    }
    assert(0);
}


/**
 * Allocate memory in ``coldata`` for ``nrows`` elements of type ``coltype``.
 * @returns 0 on success, -1 on error
 */
static int _allocate_column(dt_Coltype *coltype, dt_Coldata *coldata, long nrows) {
    switch (*coltype) {
        case DT_DOUBLE:
            coldata->ddouble = (double*) malloc(sizeof(double) * nrows);
            return coldata->ddouble == NULL? -1 : 0;
        case DT_LONG:
            coldata->dlong = (long*) malloc(sizeof(long) * nrows);
            return coldata->dlong == NULL? -1 : 0;
        case DT_BOOL:
            coldata->dbool = (unsigned char*) malloc(sizeof(char) * nrows);
            return coldata->dbool == NULL? -1 : 0;
        case DT_STRING:
            coldata->dstring = (char**) malloc(sizeof(char*) * nrows);
            return coldata->dstring == NULL? -1 : 0;
        case DT_OBJECT:
            coldata->dobject = (PyObject**) malloc(sizeof(PyObject*) * nrows);
            return coldata->dobject == NULL? -1 : 0;
        case DT_AUTO:
            /* Don't do anything -- not an instantiable column type */
            return 0;
    }
    assert(0);
}


/**
 * Free memory in ``coldata`` of type ``coltype``.
 *
 * Note: when called on a DT_OBJECT column, this will not Py_DECREF inddividual
 * elements, since this function does not know how many of those elements were
 * actually put in.
 */
static void _deallocate_column(dt_Coltype *coltype, dt_Coldata *coldata) {
    switch (*coltype) {
        case DT_DOUBLE: free(coldata->ddouble);  return;
        case DT_LONG:   free(coldata->dlong);    return;
        case DT_BOOL:   free(coldata->dbool);    return;
        case DT_STRING: free(coldata->dstring);  return;
        case DT_OBJECT: free(coldata->dobject);  return;
        case DT_AUTO:   /* Noop */               return;
    }
    assert(0);
}


/**
 * Switch to a different column type and then re-run `_fill_1_column()`.
 */
static int _switch_to_coltype(dt_Coltype newtype, PyObject *list, dt_Coltype *coltype, dt_Coldata *coldata) {
    _deallocate_column(coltype, coldata);
    *coltype = newtype;
    return _fill_1_column(list, coltype, coldata);
}



//======================================================================================================================
// Other Datatable methods
//======================================================================================================================

static void dt_Datatable_dealloc(dt_DatatableObject *self)
{
    if (self->ncols > 0) {
        for (int i = 0; i < self->ncols; ++i) {
            dt_Coltype ctype = self->coltypes[i];
            dt_Coldata cdata = self->columns[i];
            switch (ctype) {
                case DT_DOUBLE:
                    free(cdata.ddouble);
                    break;
                case DT_LONG:
                    free(cdata.dlong);
                    break;
                case DT_STRING:
                    for (int j = self->nrows - 1; j >= 0; --j) {
                        free(cdata.dstring[j]);
                    }
                    free(cdata.dstring);
                    break;
                case DT_BOOL:
                    free(cdata.dbool);
                    break;
                case DT_OBJECT:
                    for (int j = self->nrows - 1; j >= 0; --j) {
                        Py_XDECREF(cdata.dobject[j]);
                    }
                    free(cdata.dobject);
                    break;
                case DT_AUTO:
                    /* error */
                    break;
            }
        }
    }
    free(self->coltypes);
    free(self->columns);
    Py_TYPE(self)->tp_free((PyObject*)self);
}


static PyObject* dt_Datatable_view(dt_DatatableObject *self, PyObject *args)
{
    int col0, ncols, nrows;
    long row0;
    if (!PyArg_ParseTuple(args, "iili", &col0, &ncols, &row0, &nrows))
        return NULL;

    if (col0 < 0 || col0 + ncols > self->ncols || row0 < 0 || row0 + nrows > self->nrows) {
        PyErr_SetString(PyExc_ValueError, "Invalid data window bounds");
        return NULL;
    }

    PyObject *view = PyList_New((Py_ssize_t) ncols);
    if (view == NULL) return NULL;

    PyObject *types = PyList_New((Py_ssize_t) ncols);
    if (types == NULL) goto fail;

    for (Py_ssize_t i = 0; i < ncols; ++i) {
        dt_Coltype coltype = self->coltypes[col0 + i];
        dt_Coldata coldata = self->columns[col0 + i];
        PyList_SET_ITEM(types, i, PyLong_FromLong(coltype));
        PyObject *collist = PyList_New((Py_ssize_t) nrows);
        if (collist == NULL) goto fail;
        PyList_SET_ITEM(view, i, collist);
        PyObject *value;
        for (Py_ssize_t j = 0; j < nrows; ++j) {
            switch (coltype) {
                case DT_DOUBLE: {
                    double x = coldata.ddouble[row0 + j];
                    value = isnan(x) ? none() : (PyObject*) PyFloat_FromDouble(x);
                }   break;

                case DT_LONG: {
                    long x = coldata.dlong[row0 + j];
                    value = x == LONG_MIN? none() : (PyObject*) PyLong_FromLong(x);
                }   break;

                case DT_STRING: {
                    char* x = coldata.dstring[row0 + j];
                    value = x == NULL? none() : (PyObject*) PyUnicode_FromString(x);
                }   break;

                case DT_BOOL: {
                    unsigned char x = coldata.dbool[row0 + j];
                    value = x == 0? Py_int0 : x == 1? Py_int1 : Py_None;
                    Py_INCREF(value);
                }   break;

                case DT_OBJECT:
                    value = coldata.dobject[row0 + j];
                    Py_INCREF(value);
                    break;

                case DT_AUTO:
                    PyErr_SetString(PyExc_RuntimeError, "Internal error: column of type DT_AUTO found");
                    goto fail;
            }
            PyList_SET_ITEM(collist, j, value);
        }
    }

    dt_DtViewObject *res = (dt_DtViewObject*) dt_DtViewType.tp_alloc(&dt_DtViewType, 0);
    if (res == NULL) goto fail;

    res->col0 = col0;
    res->ncols = ncols;
    res->row0 = row0;
    res->nrows = nrows;
    res->types = types;
    res->data = view;
    return (PyObject*) res;

fail:
    Py_XDECREF(view);
    Py_XDECREF(types);
    return NULL;
}


PyDoc_STRVAR(dtdoc_view, "Retrieve datatable's data within a window");
PyDoc_STRVAR(dtdoc_ncols, "Number of columns");
PyDoc_STRVAR(dtdoc_col0, "Index of the first column");
PyDoc_STRVAR(dtdoc_nrows, "Number of rows");
PyDoc_STRVAR(dtdoc_row0, "Index of the first row");
PyDoc_STRVAR(dtdoc_viewtypes, "Types of the columns within the view");
PyDoc_STRVAR(dtdoc_viewdata, "Datatable's data within the specified window");
PyDoc_STRVAR(dtdoc_fromlist, "Create Datatable from a list");


static PyMethodDef dt_Datatable_methods[] = {
    {"window", (PyCFunction)dt_Datatable_view, METH_VARARGS, dtdoc_view},
    {"from_list", (PyCFunction)dt_Datatable_fromlist, METH_VARARGS | METH_CLASS, dtdoc_fromlist},
    {NULL, NULL}           /* sentinel */
};

static PyMemberDef dt_Datatable_members[] = {
    {"ncols", T_INT,  offsetof(dt_DatatableObject, ncols), READONLY, dtdoc_ncols},
    {"nrows", T_LONG, offsetof(dt_DatatableObject, nrows), READONLY, dtdoc_nrows},
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




/*------------------------------------------------------------------------------
 * DtView type
 *----------------------------------------------------------------------------*/


static void dt_DtView_dealloc(dt_DtViewObject *self)
{
    Py_XDECREF(self->data);
    Py_XDECREF(self->types);
    Py_TYPE(self)->tp_free((PyObject*)self);
}


static PyMemberDef dt_DtView_members[] = {
    {"ncols", T_INT,  offsetof(dt_DtViewObject, ncols), READONLY, dtdoc_ncols},
    {"nrows", T_INT,  offsetof(dt_DtViewObject, nrows), READONLY, dtdoc_nrows},
    {"col0",  T_INT,  offsetof(dt_DtViewObject, col0), READONLY, dtdoc_col0},
    {"row0",  T_LONG, offsetof(dt_DtViewObject, row0), READONLY, dtdoc_row0},
    {"types", T_OBJECT_EX, offsetof(dt_DtViewObject, types), READONLY, dtdoc_viewtypes},
    {"data",  T_OBJECT_EX, offsetof(dt_DtViewObject, data), READONLY, dtdoc_viewdata},
    {NULL}                 /* sentinel */
};


PyTypeObject dt_DtViewType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "_datatable.DataView",              /* tp_name */
    sizeof(dt_DtViewObject),            /* tp_basicsize */
    0,                                  /* tp_itemsize */
    (destructor)dt_DtView_dealloc,      /* tp_dealloc */
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
    "DtView object",                    /* tp_doc */
    0,                                  /* tp_traverse */
    0,                                  /* tp_clear */
    0,                                  /* tp_richcompare */
    0,                                  /* tp_weaklistoffset */
    0,                                  /* tp_iter */
    0,                                  /* tp_iternext */
    0,                                  /* tp_methods */
    dt_DtView_members,                  /* tp_members */
};
