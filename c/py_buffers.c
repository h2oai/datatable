#include "column.h"
#include "py_datatable.h"
#include "py_types.h"
#include "py_utils.h"


/**
 *
 */
PyObject* pydatatable_from_buffers(UU, PyObject *args)
{
    DataTable *dt = NULL;
    PyObject *list = NULL;
    Column **columns = NULL;

    if (!PyArg_ParseTuple(args, "O!:from_buffers", &PyList_Type, &list))
        return NULL;

    int n = (int) PyList_Size(list);
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
        Py_buffer view;
        view.buf = NULL;
        int ret = PyObject_GetBuffer(item, &view, PyBUF_FORMAT | PyBUF_ND | PyBUF_WRITABLE);
        if (ret != 0) return NULL;

    }
}
