#include "py_datatable.h"
#include "py_rowmapping.h"
#include "datatable.h"
#include "utils.h"


#define VALASSERT(test, ...)                                                   \
    if (!(test)) {                                                             \
        PyErr_Format(PyExc_ValueError, __VA_ARGS__);                           \
        return NULL;                                                           \
    }


/**
 * Create a new RowMapping_PyObject by wrapping the provided RowMapping
 * object `src`. The returned object will assume ownership of `src`. If `src`
 * is NULL then this function also returns NULL.
 */
static PyObject* py(RowMapping *src)
{
    if (src == NULL) return NULL;
    PyObject *res = PyObject_CallObject((PyObject*) &RowMapping_PyType, NULL);
    ((RowMapping_PyObject*) res)->ref = src;
    return res;
}



//==============================================================================

/**
 * Construct a RowMapping object from a list of tuples (start, count, step)
 * that are given in the form of 3 arrays start[], count[], step[].
 * This is a Python wrapper for :func:`rowmapping_from_slicelist`.
 *
 * TODO: merge with pyrowmapping_from_pyslicelist
 */
RowMapping* rowmapping_from_pyslicelist(PyObject *pystarts, PyObject *pycounts,
                                        PyObject *pysteps)
{
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

    return rowmapping_from_slicelist(starts, counts, steps, n1);

  fail:
    free(starts);
    free(counts);
    free(steps);
    return NULL;
}



/**
 * Construct RowMapping object from an array of indices. This is a wrapper
 * for :func:`rowmapping_from_i32_array` / :func:`rowmapping_from_i64_array`.
 */
RowMapping* rowmapping_from_pyarray(PyObject *list)
{
    int32_t *data32 = NULL;
    int64_t *data64 = NULL;

    // Convert Pythonic List into a regular C array of int32's/int64's
    int64_t len = PyList_Size(list);
    data32 = TRY(malloc(4 * (size_t)len));
    for (int64_t i = 0; i < len; i++) {
        int64_t x = PyLong_AsSsize_t(PyList_GET_ITEM(list, i));
        if (x == -1 && PyErr_Occurred()) goto fail;
        VALASSERT(x >= 0, "Negative indices not allowed: %zd", x)
        if (data64) {
            data64[i] = x;
        } else if (x <= INT32_MAX) {
            data32[i] = (int32_t) x;
        } else {
            data64 = TRY(malloc(8 * (size_t)len));
            for (int64_t j = 0; j < i; j++)
                data64[j] = (int64_t) data32[j];
            free(data32);
            data32 = NULL;
            data64[i] = x;
        }
    }

    // Construct and return the RowMapping object
    return data32? rowmapping_from_i32_array(data32, len)
                 : rowmapping_from_i64_array(data64, len);

  fail:
    free(data32);
    free(data64);
    return NULL;
}



//==============================================================================

/**
 * Construct a (py)RowMapping "slice" object given a tuple (start, count, step).
 * This is a Python wrapper for :func:`rowmapping_from_slice`.
 */
PyObject* pyrowmapping_from_slice(PyObject *self, PyObject *args)
{
    int64_t start, count, step;
    if (!PyArg_ParseTuple(args, "LLL:RowMapping.from_slice",
                          &start, &count, &step))
        return NULL;
    return py(rowmapping_from_slice(start, count, step));
}



/**
 * Python wrapper for :func:`rowmapping_from_pyslicelist`.
 */
PyObject* pyrowmapping_from_slicelist(PyObject *self, PyObject *args)
{
    PyObject *pystarts, *pycounts, *pysteps;
    if (!PyArg_ParseTuple(args, "O!O!O!:RowMapping.from_slicelist",
                          &PyList_Type, &pystarts, &PyList_Type, &pycounts,
                          &PyList_Type, &pysteps))
        return NULL;
    return py(rowmapping_from_pyslicelist(pystarts, pycounts, pysteps));
}



/**
 * Python wrapper for `rowmapping_from_pyarray`.
 */
PyObject* pyrowmapping_from_array(PyObject *self, PyObject *args)
{
    PyObject *list;
    if (!PyArg_ParseTuple(args, "O!:RowMapping.from_array",
                          &PyList_Type, &list))
        return NULL;
    return py(rowmapping_from_pyarray(list));
}



/**
 * Construct a RowMapping object given a DataTable with a single boolean column.
 * This column is then treated as a filter, and the RowMapping is constructed
 * with the indices that corresponds to the rows where the boolean column has
 * true values (all false / NA columns are skipped).
 */
PyObject* pyrowmapping_from_column(PyObject *self, PyObject *args)
{
    DataTable *dt = NULL;
    RowMapping *rowmapping = NULL;
    if (!PyArg_ParseTuple(args, "O&:RowMapping.from_column",
                          &dt_from_pydt, &dt))
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

    rowmapping = (col->mtype == MT_VIEW)
        ? rowmapping_from_column_with_rowmapping(
            dt->source->columns[((ViewColumn*)col)->srcindex], dt->rowmapping)
        : rowmapping_from_datacolumn(col, dt->nrows);

    return py(rowmapping);
}




//==============================================================================
//  RowMapping PyObject
//==============================================================================

static void dealloc(RowMapping_PyObject *self)
{
    rowmapping_dealloc(self->ref);
    Py_TYPE(self)->tp_free((PyObject*)self);
}


static PyObject* repr(RowMapping_PyObject *self)
{
    RowMapping *rwm = self->ref;
    if (rwm == NULL)
        return PyUnicode_FromString("_RowMapping(NULL)");
    if (rwm->type == RM_ARR32) {
        return PyUnicode_FromFormat("_RowMapping(int32[%ld])", rwm->length);
    }
    if (rwm->type == RM_ARR64) {
        return PyUnicode_FromFormat("_RowMapping(int64[%ld])", rwm->length);
    }
    if (rwm->type == RM_SLICE) {
        return PyUnicode_FromFormat("_RowMapping(%ld:%ld:%ld)",
            rwm->slice.start, rwm->length, rwm->slice.step);
    }
    return NULL;
}



PyTypeObject RowMapping_PyType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "_datatable.RowMapping",            /* tp_name */
    sizeof(RowMapping_PyObject),        /* tp_basicsize */
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
    0,                                  /* tp_methods */
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


// Add PyRowMapping object to the Python module
int init_py_rowmapping(PyObject *module)
{
    RowMapping_PyType.tp_new = PyType_GenericNew;
    if (PyType_Ready(&RowMapping_PyType) < 0) return 0;
    Py_INCREF(&RowMapping_PyType);
    PyModule_AddObject(module, "RowMapping", (PyObject*) &RowMapping_PyType);
    return 1;
}
