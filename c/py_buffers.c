#include "column.h"
#include "py_datatable.h"
#include "py_types.h"
#include "py_utils.h"


/**
 * Load datatable from a list of Python objects supporting Buffers protocol.
 *
 * See: https://docs.python.org/3/c-api/buffer.html
 */
PyObject* pydatatable_from_buffers(UU, PyObject *args)
{
    PyObject *list = NULL;

    if (!PyArg_ParseTuple(args, "O!:from_buffers", &PyList_Type, &list))
        return NULL;

    int n = (int) PyList_Size(list);
    Column **columns = NULL;
    dtmalloc(columns, Column*, n + 1);
    columns[n] = NULL;

    for (int i = 0; i < n; i++) {
        PyObject *item = PyList_GET_ITEM(list, i);
        if (!PyObject_CheckBuffer(item)) {
            PyErr_Format(PyExc_ValueError,
                "Element %d in the list of sources does not support buffers "
                "interface", i);
            return NULL;
        }
        Py_buffer *view;
        dtcalloc(view, Py_buffer, 1);

        // Request the buffer (not writeable). Flag PyBUF_FORMAT indicates that
        // the `view->format` field should be filled; and PyBUF_ND will fill the
        // `view->shape` information (while `strides` and `suboffsets` will be
        // NULL).
        int ret = PyObject_GetBuffer(item, view, PyBUF_FORMAT | PyBUF_ND);
        if (ret != 0) {
            PyErr_Format(PyExc_ValueError,
                "Unable to retrieve buffer from item %d", i);
            return NULL;
        }
        if (view->ndim != 1) {
            PyErr_Format(PyExc_ValueError,
                "Buffer returned has ndim=%d, cannot handle", view->ndim);
            return NULL;
        }

        columns[i] = column_from_buffer(view, view->buf, (size_t) view->len,
                                        (size_t) view->itemsize, view->format);
        if (columns[i] == NULL) return NULL;
    }

    DataTable *dt = make_datatable(columns, NULL);
    return pydt_from_dt(dt);
}
