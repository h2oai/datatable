
#include "rows.h"


PyObject* rows_from_slice(PyObject *self, PyObject *args)
{
    long start, count, step;
    if (!PyArg_ParseTuple(args, "lll:rows_slice", &start, &count, &step))
        return NULL;

    if (start < 0 || count < 0) {
        PyErr_SetString(PyExc_ValueError,
                        "`start` and `count` must be nonnegative");
        return NULL;
    }

    PyTypeObject* type = &dt_RowsIndexType;
    dt_RowsIndexObject *res = (dt_RowsIndexObject*) type->tp_alloc(type, 0);
    if (res == NULL)
        return NULL;

    res->kind = RI_SLICE;
    res->riSlice.start = start;
    res->riSlice.count = count;
    res->riSlice.step = step;
    return (PyObject*) res;
}


PyObject* rows_from_array(PyObject *self, PyObject *args)
{
    PyObject *list;
    if (!PyArg_ParseTuple(args, "O!:rows_from_array", &PyList_Type, &list))
        return NULL;

    long len = (long) PyList_Size(list);
    long *data = (long*) malloc(sizeof(long) * len);
    if (data == NULL) {
        PyErr_SetString(PyExc_RuntimeError, "Failed to allocate memory");
        return NULL;
    }

    for (long i = 0, *ptr = data; i < len; ++i, ++ptr) {
        PyObject *elem = PyList_GET_ITEM(list, i);
        *ptr = PyLong_AsLong(elem);
    }

    PyTypeObject* type = &dt_RowsIndexType;
    dt_RowsIndexObject *res = (dt_RowsIndexObject*) type->tp_alloc(type, 0);
    if (res == NULL) {
        free(data);
        return NULL;
    }

    res->kind = RI_ARRAY;
    res->riArray.length = len;
    res->riArray.rows = data;
    return (PyObject*) res;
}



//------ RowsIndex -------------------------------------------------------------

static void dt_RowsIndex_dealloc(dt_RowsIndexObject *self)
{
    if (self->kind == RI_ARRAY)
        free(self->riArray.rows);
    Py_TYPE(self)->tp_free((PyObject*)self);
}


PyTypeObject dt_RowsIndexType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "_datatable.RowsIndex",             /* tp_name */
    sizeof(dt_RowsIndexObject),         /* tp_basicsize */
    0,                                  /* tp_itemsize */
    (destructor)dt_RowsIndex_dealloc,   /* tp_dealloc */
};
