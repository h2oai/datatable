#include "rows.h"
#include "datatable.h"


/**
 * Construct a RowsIndex object from the given slice applied to the provided
 * datatable. If the datatable is a view on another datatable, then the
 * returned RowsIndex will refer to the datatable's parent.
 */
PyObject* rows_from_slice(PyObject *self, PyObject *args)
{
    long start, count, step;
    dt_DatatableObject *dt;
    if (!PyArg_ParseTuple(args, "O!lll:rows_slice",
                          &dt_DatatableType, &dt, &start, &count, &step))
        return NULL;

    if (start < 0 || count < 0) {
        PyErr_SetString(PyExc_ValueError,
                        "`start` and `count` must be nonnegative");
        return NULL;
    }

    dt_RowIndexObject *res = dt_RowsIndex_NEW();
    if (res == NULL) return NULL;

    if (dt->row_index == NULL) {
        res->kind = RI_SLICE;
        res->length = count;
        res->slice.start = start;
        res->slice.step = step;
    }
    else if (dt->row_index->kind == RI_SLICE) {
        long srcstart = dt->row_index->slice.start;
        long srcstep = dt->row_index->slice.step;
        res->kind = RI_SLICE;
        res->length = count;
        res->slice.start = srcstart + srcstep * start;
        res->slice.step = step * srcstep;
    }
    else if (dt->row_index->kind == RI_ARRAY) {
        long *data = malloc(sizeof(long) * count);
        if (data == NULL) {
            Py_DECREF(res);
            return NULL;
        }
        long *srcrows = dt->row_index->array;
        for (long i = 0; i < count; ++i) {
            data[i] = srcrows[start + i * step];
        }
        res->kind = RI_ARRAY;
        res->length = count;
        res->array = data;
    } else assert(0);
    return (PyObject*) res;
}


/**
 * Construct a RowsIndex object from the given list of row indices, applied to
 * the provided datatable. If the datatable is a view on another datatable,
 * then the returned RowsIndex will refer to the parent.
 */
PyObject* rows_from_array(PyObject *self, PyObject *args)
{
    dt_DatatableObject *dt;
    PyObject *list;
    if (!PyArg_ParseTuple(args, "O!O!:rows_from_array",
                          &dt_DatatableType, &dt, &PyList_Type, &list))
        return NULL;

    dt_RowIndexObject *res = dt_RowsIndex_NEW();
    if (res == NULL) return NULL;

    long len = (long) PyList_Size(list);
    long *data = malloc(sizeof(long) * len);
    if (data == NULL) {
        PyErr_SetString(PyExc_RuntimeError, "Failed to allocate memory");
        Py_DECREF(res);
        return NULL;
    }

    if (dt->row_index == NULL) {
        for (long i = 0; i < len; ++i) {
            data[i] = PyLong_AsLong(PyList_GET_ITEM(list, i));
        }
    }
    else if (dt->row_index->kind == RI_SLICE) {
        long srcstart = dt->row_index->slice.start;
        long srcstep = dt->row_index->slice.step;
        for (long i = 0; i < len; ++i) {
            long x = PyLong_AsLong(PyList_GET_ITEM(list, i));
            data[i] = srcstart + x * srcstep;
        }
    }
    else if (dt->row_index->kind == RI_ARRAY) {
        long *srcrows = dt->row_index->array;
        for (long i = 0; i < len; ++i) {
            long x = PyLong_AsLong(PyList_GET_ITEM(list, i));
            data[i] = srcrows[x];
        }
    } else assert(0);

    res->kind = RI_ARRAY;
    res->length = len;
    res->array = data;
    return (PyObject*) res;
}



//------ RowsIndex -------------------------------------------------------------

static void dt_RowsIndex_dealloc(dt_RowIndexObject *self)
{
    if (self->kind == RI_ARRAY)
        free(self->array);
    Py_TYPE(self)->tp_free((PyObject*)self);
}


PyTypeObject dt_RowIndexType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "_datatable.RowsIndex",             /* tp_name */
    sizeof(dt_RowIndexObject),         /* tp_basicsize */
    0,                                  /* tp_itemsize */
    (destructor)dt_RowsIndex_dealloc,   /* tp_dealloc */
};
