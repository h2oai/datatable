#include "py_datatable.h"
#include "py_rowindex.h"
#include "py_utils.h"


#define VALASSERT(test, ...)                                                   \
    if (!(test)) {                                                             \
        PyErr_Format(PyExc_ValueError, __VA_ARGS__);                           \
        return NULL;                                                           \
    }


/**
 * Create a new RowIndex_PyObject by wrapping the provided RowIndex `src`.
 * The returned py-object will hold a durable reference to `src`; for example
 * if `src` is a RowIndex within a DataTable, there is no danger of having the
 * reference become invalid if the DataTable is garbage-collected.
 *
 * If `src` is NULL then this function also returns NULL.
 */
PyObject* pyrowindex(RowIndex *src)
{
    if (src == NULL) return NULL;
    PyObject *res = PyObject_CallObject((PyObject*) &RowIndex_PyType, NULL);
    ((RowIndex_PyObject*) res)->ref = rowindex_incref(src);
    return res;
}
#define py pyrowindex


/**
 * Helper function to be used with `PyArg_ParseTuple()` in order to extract
 * a `RowIndex` object out of the arguments tuple. Usage:
 *
 *     RowIndex *ri;
 *     if (!PyArg_ParseTuple(args, "O&", &rowindex_unwrap, &ri))
 *         return NULL;
 *
 * The returned reference is *borrowed*, i.e. the caller is not expected to
 * decref it.
 */
int rowindex_unwrap(PyObject *object, void *address) {
    RowIndex **ans = address;
    if (object == Py_None) {
        *ans = NULL;
        return 1;
    }
    if (!PyObject_TypeCheck(object, &RowIndex_PyType)) {
        PyErr_SetString(PyExc_TypeError,
                        "Expected argument of type RowIndex");
        return 0;
    }
    *ans = ((RowIndex_PyObject*)object)->ref;
    return 1;
}




//==============================================================================

/**
 * Construct a (py)RowIndex "slice" object given a tuple (start, count, step).
 * This is a Python wrapper for :func:`rowindex_from_slice`.
 */
PyObject* pyrowindex_from_slice(UU, PyObject *args)
{
    int64_t start, count, step;
    if (!PyArg_ParseTuple(args, "LLL:RowIndex.from_slice",
                          &start, &count, &step))
        return NULL;
    return py(rowindex_from_slice(start, count, step));
}



/**
 * Construct a RowIndex object from a list of tuples (start, count, step)
 * that are given in the form of 3 arrays start[], count[], step[].
 * This is a Python wrapper for :func:`rowindex_from_slicelist`.
 */
PyObject* pyrowindex_from_slicelist(UU, PyObject *args)
{
    PyObject *pystarts, *pycounts, *pysteps;
    if (!PyArg_ParseTuple(args, "O!O!O!:RowIndex.from_slicelist",
                          &PyList_Type, &pystarts,
                          &PyList_Type, &pycounts,
                          &PyList_Type, &pysteps))
        return NULL;

    int64_t *starts = NULL, *counts = NULL, *steps = NULL;
    int64_t n1 = PyList_Size(pystarts);
    int64_t n2 = PyList_Size(pycounts);
    int64_t n3 = PyList_Size(pysteps);
    VALASSERT(n1 >= n2, "counts array cannot be longer than the starts array")
    VALASSERT(n1 >= n3, "steps array cannot be longer than the starts array")
    starts = TRY(malloc(sizeof(int64_t) * (size_t)n1));
    counts = TRY(malloc(sizeof(int64_t) * (size_t)n1));
    steps  = TRY(malloc(sizeof(int64_t) * (size_t)n1));

    // Convert Pythonic lists into regular C arrays of longs
    int64_t start, count, step;
    for (int64_t i = 0; i < n1; i++) {
        start = PyLong_AsSsize_t(PyList_GET_ITEM(pystarts, i));
        count = i < n2? PyLong_AsSsize_t(PyList_GET_ITEM(pycounts, i)) : 1;
        step  = i < n3? PyLong_AsSsize_t(PyList_GET_ITEM(pysteps, i)) : 1;
        if ((start == -1 || count  == -1 || step == -1) &&
            PyErr_Occurred()) goto fail;
        starts[i] = start;
        counts[i] = count;
        steps[i] = step;
    }

    return py(rowindex_from_slicelist(starts, counts, steps, n1));

  fail:
    free(starts);
    free(counts);
    free(steps);
    return NULL;
}



/**
 * Construct RowIndex object from an array of indices. This is a wrapper
 * for :func:`rowindex_from_i32_array` / :func:`rowindex_from_i64_array`.
 */
PyObject* pyrowindex_from_array(UU, PyObject *args)
{
    PyObject *list;
    if (!PyArg_ParseTuple(args, "O!:RowIndex.from_array",
                          &PyList_Type, &list))
        return NULL;

    int32_t *data32 = NULL;
    int64_t *data64 = NULL;

    // Convert Pythonic List into a regular C array of int32's/int64's
    int64_t len = PyList_Size(list);
    dtmalloc(data32, int32_t, len);
    for (int64_t i = 0; i < len; i++) {
        int64_t x = PyLong_AsSsize_t(PyList_GET_ITEM(list, i));
        if (x == -1 && PyErr_Occurred()) goto fail;
        VALASSERT(x >= 0, "Negative indices not allowed: %zd", x)
        if (data64) {
            data64[i] = x;
        } else if (x <= INT32_MAX) {
            data32[i] = (int32_t) x;
        } else {
            dtmalloc(data64, int64_t, len);
            for (int64_t j = 0; j < i; j++)
                data64[j] = (int64_t) data32[j];
            free(data32);
            data32 = NULL;
            data64[i] = x;
        }
    }

    // Construct and return the RowIndex object
    return data32? py(rowindex_from_i32_array(data32, len, 0))
                 : py(rowindex_from_i64_array(data64, len, 0));

  fail:
    dtfree(data32);
    dtfree(data64);
    return NULL;
}



/**
 * Construct a RowIndex object given a DataTable with a single boolean column.
 * This column is then treated as a filter, and the RowIndex is constructed
 * with the indices that corresponds to the rows where the boolean column has
 * true values (all false / NA columns are skipped).
 */
PyObject* pyrowindex_from_boolcolumn(UU, PyObject *args)
{
    DataTable *dt = NULL;
    if (!PyArg_ParseTuple(args, "O&:RowIndex.from_boolcolumn", &dt_unwrap, &dt))
        return NULL;

    if (dt->ncols != 1) {
        PyErr_SetString(PyExc_ValueError, "Expected a single-column datatable");
        return NULL;
    }
    Column *col = dt->columns[0];
    if (col->stype != ST_BOOLEAN_I1) {
        PyErr_SetString(PyExc_ValueError, "A boolean column is required");
        return NULL;
    }

    RowIndex *rowindex = dt->rowindex
        ? rowindex_from_boolcolumn_with_rowindex(col, dt->rowindex)
        : rowindex_from_boolcolumn(col, dt->nrows);

    return py(rowindex);
}



/**
 * Construct a RowIndex object from a DataTable having a single integer column.
 * This column will be converted into a RowIndex directly.
 */
PyObject* pyrowindex_from_intcolumn(UU, PyObject *args)
{
    DataTable *dt = NULL;
    long target_nrows = 0;
    if (!PyArg_ParseTuple(args, "O&l:RowIndex.from_intcolumn",
                          &dt_unwrap, &dt, &target_nrows))
        return NULL;

    if (dt->ncols != 1) {
        PyErr_SetString(PyExc_ValueError, "Expected a single-column datatable");
        return NULL;
    }
    Column *col = dt->columns[0];
    if (stype_info[col->stype].ltype != LT_INTEGER) {
        PyErr_SetString(PyExc_ValueError, "An integer column is required");
        return NULL;
    }

    RowIndex *rowindex = dt->rowindex
        ? rowindex_from_intcolumn_with_rowindex(col, dt->rowindex)
        : rowindex_from_intcolumn(col, 0);

    if (rowindex->min < 0 || rowindex->max >= target_nrows) {
        PyErr_Format(PyExc_ValueError,
            "The data column contains NAs or indices that are outside of the "
            "allowed range [0 .. %lld)", target_nrows);
        return NULL;
    }
    return py(rowindex);
}



/**
 * Construct a rowindex object given a pointer to a filtering function and
 * the number of rows that has to be filtered. This is a wrapper around
 * `rowindex_from_filterfn[32|64]`.
 */
PyObject* pyrowindex_from_filterfn(UU, PyObject *args)
{
    long long _fnptr, _nrows;
    if (!PyArg_ParseTuple(args, "LL:RowIndex.from_filterfn",
                          &_fnptr, &_nrows))
        return NULL;

    int64_t nrows = (int64_t) _nrows;
    if (nrows <= INT32_MAX) {
        rowindex_filterfn32 *fnptr = (rowindex_filterfn32*)_fnptr;
        return py(rowindex_from_filterfn32(fnptr, nrows, 0));
    } else {
        rowindex_filterfn64 *fnptr = (rowindex_filterfn64*)_fnptr;
        return py(rowindex_from_filterfn64(fnptr, nrows, 0));
    }
}



/**
 * Construct a rowindex object given a pointer to a function that returns a
 * `RowIndex*` value.
 */
PyObject* pyrowindex_from_function(UU, PyObject *args)
{
    long long _fnptr;
    if (!PyArg_ParseTuple(args, "L:RowIndex.from_function", &_fnptr))
        return NULL;
    rowindex_getterfn *fnptr = (rowindex_getterfn*) _fnptr;
    return py(fnptr());
}



PyObject* pyrowindex_uplift(UU, PyObject *args)
{
    RowIndex *ri;
    DataTable *dt;
    if (!PyArg_ParseTuple(args, "O&O&:RowIndex.uplift",
                          &rowindex_unwrap, &ri, &dt_unwrap, &dt))
        return NULL;
    return py(rowindex_merge(dt->rowindex, ri));
}



//==============================================================================
//  RowIndex PyObject
//==============================================================================

static void dealloc(RowIndex_PyObject *self)
{
    rowindex_decref(self->ref);
    Py_TYPE(self)->tp_free((PyObject*)self);
}


static PyObject* repr(RowIndex_PyObject *self)
{
    RowIndex *ri = self->ref;
    if (ri == NULL)
        return PyUnicode_FromString("_RowIndex(NULL)");
    if (ri->type == RI_ARR32) {
        return PyUnicode_FromFormat("_RowIndex(int32[%ld])", ri->length);
    }
    if (ri->type == RI_ARR64) {
        return PyUnicode_FromFormat("_RowIndex(int64[%ld])", ri->length);
    }
    if (ri->type == RI_SLICE) {
        return PyUnicode_FromFormat("_RowIndex(%ld:%ld:%ld)",
            ri->slice.start, ri->length, ri->slice.step);
    }
    return NULL;
}


static PyObject* tolist(RowIndex_PyObject *self, PyObject *args)
{
    if (!PyArg_ParseTuple(args, "")) return NULL;
    RowIndex *ri = self->ref;

    PyObject *list = PyList_New((Py_ssize_t) ri->length);
    switch (ri->type) {
        case RI_ARR32: {
            int32_t n = (int32_t) ri->length;
            int32_t *a = ri->ind32;
            for (int32_t i = 0; i < n; i++) {
                PyList_SET_ITEM(list, i, PyLong_FromLong(a[i]));
            }
        } break;
        case RI_ARR64: {
            int64_t n = ri->length;
            int64_t *a = ri->ind64;
            for (int64_t i = 0; i < n; i++) {
                PyList_SET_ITEM(list, i, PyLong_FromLong(a[i]));
            }
        } break;
        case RI_SLICE: {
            int64_t n = ri->length;
            int64_t start = ri->slice.start;
            int64_t step = ri->slice.step;
            for (int64_t i = 0; i < n; i++) {
                PyList_SET_ITEM(list, i, PyLong_FromLong(start + i*step));
            }
        }
    }
    return list;
}


static PyObject *getptr(RowIndex_PyObject *self, UU)
{
    RowIndex *ri = self->ref;
    return PyLong_FromSize_t((size_t) ri);
}



//==============================================================================
// DataTable type definition
//==============================================================================

#define METHOD0(name) {#name, (PyCFunction)name, METH_VARARGS, NULL}
#define METHOD1(name) {#name, (PyCFunction)name, METH_NOARGS, NULL}
static PyMethodDef rowindex_methods[] = {
    METHOD0(tolist),
    METHOD1(getptr),
    {NULL, NULL, 0, NULL}           /* sentinel */
};


PyTypeObject RowIndex_PyType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "_datatable.RowIndex",              /* tp_name */
    sizeof(RowIndex_PyObject),          /* tp_basicsize */
    0,                                  /* tp_itemsize */
    (destructor)dealloc,                /* tp_dealloc */
    0,                                  /* tp_print */
    0,                                  /* tp_getattr */
    0,                                  /* tp_setattr */
    0,                                  /* tp_compare */
    (reprfunc)repr,                     /* tp_repr */
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
    0,                                  /* tp_doc */
    0,                                  /* tp_traverse */
    0,                                  /* tp_clear */
    0,                                  /* tp_richcompare */
    0,                                  /* tp_weaklistoffset */
    0,                                  /* tp_iter */
    0,                                  /* tp_iternext */
    rowindex_methods,                   /* tp_methods */
    0,                                  /* tp_members */
    0,                                  /* tp_getset */
    0,                                  /* tp_base */
    0,                                  /* tp_dict */
    0,                                  /* tp_descr_get */
    0,                                  /* tp_descr_set */
    0,                                  /* tp_dictoffset */
    0,                                  /* tp_init */
    0,0,0,0,0,0,0,0,0,0,0,0
};


// Add PyRowIndex object to the Python module
int init_py_rowindex(PyObject *module)
{
    RowIndex_PyType.tp_new = PyType_GenericNew;
    if (PyType_Ready(&RowIndex_PyType) < 0) return 0;
    Py_INCREF(&RowIndex_PyType);
    PyModule_AddObject(module, "RowIndex", (PyObject*) &RowIndex_PyType);
    return 1;
}
