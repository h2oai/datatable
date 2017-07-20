#include "column.h"
#include "py_datatable.h"
#include "py_types.h"
#include "py_utils.h"

// Forward declarations
static Column* try_to_resolve_object_column(Column* col);



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
        if (columns[i]->stype == ST_OBJECT_PYPTR) {
            columns[i] = try_to_resolve_object_column(columns[i]);
        }
    }

    DataTable *dt = make_datatable(columns, NULL);
    return pydt_from_dt(dt);
}


/**
 * If a column was created from Pandas series, it may end up having
 * dtype='object' (because Pandas doesn't support string columns). This function
 * will attempt to convert such column to the string type (or some other type
 * if more appropriate), and return either the original or the new modified
 * column. If a new column is returned, the original one is decrefed.
 */
static Column* try_to_resolve_object_column(Column* col)
{
    PyObject **data = (PyObject**) col->data;
    int64_t nrows = col->nrows;

    int all_strings = 1;
    // Approximate total length of all strings. Do not take into account
    // possibility that the strings may expand in UTF-8 -- if needed, we'll
    // realloc the buffer later.
    int64_t total_length = 10;
    for (int64_t i = 0; i < nrows; i++) {
        if (data[i] == Py_None) continue;
        if (!PyUnicode_Check(data[i])) {
            all_strings = 0;
            break;
        }
        total_length += PyUnicode_GetLength(data[i]);
    }

    // Not all elements were strings -- return the original column unmodified
    if (!all_strings) {
        return col;
    }

    // Otherwise the column is all-strings: convert it into *STRING stype.
    char *strbuf = NULL;
    dtmalloc(strbuf, char, total_length);
    size_t strbuf_size = (size_t) total_length;
    Column *res = make_data_column(ST_STRING_I4_VCHAR, (size_t)nrows);
    int32_t *offsets = (int32_t*) res->data;

    int32_t offset = 0;
    for (int64_t i = 0; i < nrows; i++) {
        if (data[i] == Py_None) {
            offsets[i] = -offset - 1;
        } else {
            PyObject *z = PyUnicode_AsEncodedString(data[i], "utf-8", "strict");
            int32_t sz = (int32_t) PyBytes_Size(z);
            if ((size_t)(offset + sz) > strbuf_size) {
                strbuf_size = (size_t) (1.5 * strbuf_size);
                dtrealloc(strbuf, char, strbuf_size);
            }
            memcpy(strbuf + offset, PyBytes_AsString(z), sz);
            Py_DECREF(z);
            offset += sz;
            offsets[i] = offset + 1;
        }
    }

    size_t datasize = (size_t) offset;
    size_t padding = column_i4s_padding(datasize);
    size_t allocsize = datasize + padding + 4 * (size_t)nrows;
    dtrealloc(strbuf, char, allocsize);
    memset(strbuf + datasize, 0xFF, padding);
    memcpy(strbuf + datasize + padding, offsets, 4 * (size_t)nrows);
    dtfree(res->data);
    res->data = strbuf;
    res->alloc_size = allocsize;
    ((VarcharMeta*) res->meta)->offoff = (int64_t) (datasize + padding);
    column_decref(col);
    return res;
}
