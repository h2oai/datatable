//------------------------------------------------------------------------------
// Functionality related to "Buffers" interface
// See https://www.python.org/dev/peps/pep-3118/
//------------------------------------------------------------------------------
#include "column.h"
#include "py_column.h"
#include "py_datatable.h"
#include "py_types.h"
#include "py_utils.h"

// Forward declarations
static Column* try_to_resolve_object_column(Column* col);
static SType stype_from_format(const char *format, int64_t itemsize);

#define REQ_ND(flags)       ((flags & PyBUF_ND) == PyBUF_ND)
#define REQ_FORMAT(flags)   ((flags & PyBUF_FORMAT) == PyBUF_FORMAT)
#define REQ_STRIDES(flags)  ((flags & PyBUF_STRIDES) == PyBUF_STRIDES)
#define REQ_WRITABLE(flags) ((flags & PyBUF_WRITABLE) == PyBUF_WRITABLE)
// #define REQ_INDIRECT(flags) ((flags & PyBUF_INDIRECT) == PyBUF_INDIRECT)
// #define REQ_C_CONTIG(flags) ((flags & PyBUF_C_CONTIGUOUS) == PyBUF_C_CONTIGUOUS)
// #define REQ_F_CONTIG(flags) ((flags & PyBUF_F_CONTIGUOUS) == PyBUF_F_CONTIGUOUS)

static char strB[] = "B";


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
            PyErr_Clear();  // otherwise system functions may fail later on
            ret = PyObject_GetBuffer(item, view, PyBUF_FORMAT | PyBUF_STRIDES);
        }
        if (ret != 0) {
            if (!PyErr_Occurred())
                PyErr_Format(PyExc_ValueError,
                    "Unable to retrieve buffer for column %d", i);
            return NULL;
        }
        if (view->ndim != 1) {
            PyErr_Format(PyExc_ValueError,
                "Buffer returned has ndim=%d, cannot handle", view->ndim);
            return NULL;
        }

        SType stype = stype_from_format(view->format, view->itemsize);
        int64_t nrows = view->len / view->itemsize;
        if (stype == ST_VOID) return NULL;
        if (view->strides == NULL) {
            columns[i] = column_from_buffer(stype, nrows, view, view->buf,
                                            (size_t) view->len);
        } else {
            columns[i] = make_data_column(stype, (size_t) nrows);
            int64_t stride = view->strides[0] / view->itemsize;
            if (view->itemsize == 8) {
                int64_t *out = (int64_t*) columns[i]->data;
                int64_t *inp = (int64_t*) view->buf;
                for (int64_t j = 0; j < nrows; j++)
                    out[j] = inp[j * stride];
            } else if (view->itemsize == 4) {
                int32_t *out = (int32_t*) columns[i]->data;
                int32_t *inp = (int32_t*) view->buf;
                for (int64_t j = 0; j < nrows; j++)
                    out[j] = inp[j * stride];
            } else if (view->itemsize == 2) {
                int16_t *out = (int16_t*) columns[i]->data;
                int16_t *inp = (int16_t*) view->buf;
                for (int64_t j = 0; j < nrows; j++)
                    out[j] = inp[j * stride];
            } else if (view->itemsize == 1) {
                int8_t *out = (int8_t*) columns[i]->data;
                int8_t *inp = (int8_t*) view->buf;
                for (int64_t j = 0; j < nrows; j++)
                    out[j] = inp[j * stride];
            }
        }
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

    size_t offset = 0;
    for (int64_t i = 0; i < nrows; i++) {
        if (data[i] == Py_None) {
            offsets[i] = (int32_t)(-offset - 1);
        } else {
            PyObject *z = PyUnicode_AsEncodedString(data[i], "utf-8", "strict");
            size_t sz = (size_t) PyBytes_Size(z);
            if (offset + sz > strbuf_size) {
                strbuf_size = (size_t) (1.5 * strbuf_size);
                dtrealloc(strbuf, char, strbuf_size);
            }
            memcpy(strbuf + offset, PyBytes_AsString(z), sz);
            Py_DECREF(z);
            offset += sz;
            offsets[i] = (int32_t)(offset + 1);
        }
    }

    size_t datasize = offset;
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


//==============================================================================
// Buffers interface for Column_PyObject
//==============================================================================

/**
 * Handle a request to fill in structure `view` as specified by `flags`.
 * See https://docs.python.org/3/c-api/typeobj.html#buffer-structs for details.
 */
static int column_getbuffer(Column_PyObject *self, Py_buffer *view, int flags)
{
    Column *col = self->ref;
    Py_ssize_t *info = NULL;
    size_t elemsize = stype_info[col->stype].elemsize;
    dtmalloc_g(info, Py_ssize_t, 2);

    if (REQ_WRITABLE(flags)) {
        PyErr_SetString(PyExc_BufferError, "Cannot create writable buffer");
        goto fail;
    }
    if (stype_info[col->stype].varwidth) {
        PyErr_SetString(PyExc_BufferError, "Column's data has variable width");
        goto fail;
    }

    info[0] = (Py_ssize_t)(col->alloc_size / elemsize);
    info[1] = (Py_ssize_t) elemsize;

    view->buf = col->data;
    view->obj = (PyObject*) self;
    view->len = (Py_ssize_t) col->alloc_size;
    view->itemsize = (Py_ssize_t) elemsize;
    view->readonly = 1;
    view->ndim = 1;
    // An array of Py_ssize_t of length `ndim`, indicating the shape of the
    // memory as an n-dimensional array (prod(shape) * itemsize == len).
    // Must be provided iff PyBUF_ND is set.
    view->shape = REQ_ND(flags)? info : NULL;
    // An array of Py_ssize_t of length `ndim` giving the number of bytes to
    // skip to get to a new element in each dimension.
    // Must be provided iff PyBUF_STRIDES is set.
    view->strides = REQ_STRIDES(flags)? info + 1 : NULL;
    view->suboffsets = NULL;
    view->internal = NULL;
    view->format = REQ_FORMAT(flags) ?
        const_cast<char*>(format_from_stype(col->stype)) : NULL;

    Py_INCREF(self);
    column_incref(col);
    return 0;

  fail:
    view->obj = NULL;
    dtfree(info);
    return -1;
}

/**
 * Handle a request to release the resources of the buffer.
 */
static void column_releasebuffer(Column_PyObject *self, Py_buffer *view)
{
    dtfree(view->shape);
    column_decref(self->ref);
    // This function MUST NOT decrement view->obj, since that is done
    // automatically in PyBuffer_Release()
}

PyBufferProcs column_as_buffer = {
    (getbufferproc) column_getbuffer,
    (releasebufferproc) column_releasebuffer,
};




//==============================================================================
// Buffers interface for DataTable_PyObject
//==============================================================================

static int
dt_getbuffer_no_cols(DataTable_PyObject *self, Py_buffer *view, int flags)
{
    Py_ssize_t *info = NULL;
    if (REQ_ND(flags)) dtcalloc_g(info, Py_ssize_t, 2);
    view->buf = NULL;
    view->obj = incref((PyObject*) self);
    view->len = 0;
    view->readonly = 0;
    view->itemsize = 1;
    view->format = REQ_FORMAT(flags) ? strB : NULL;
    view->ndim = 2;
    view->shape = REQ_ND(flags) ? info : NULL;
    view->strides = REQ_STRIDES(flags) ? info : NULL;
    view->suboffsets = NULL;
    view->internal = (void*) 1;
    return 0;
    fail:
        view->obj = NULL;
        return -1;
}

static int
dt_getbuffer_1_col(DataTable_PyObject *self, Py_buffer *view, int flags)
{
    Py_ssize_t *info = NULL;
    Column *col = self->ref->columns[0];
    if (REQ_ND(flags)) dtcalloc_g(info, Py_ssize_t, 4);
    view->buf = (void*) col->data;
    view->obj = incref((PyObject*) self);
    view->len = (Py_ssize_t) col->alloc_size;
    view->readonly = 1;
    view->itemsize = (Py_ssize_t) stype_info[col->stype].elemsize;
    view->format = REQ_FORMAT(flags) ?
        const_cast<char*>(format_from_stype(col->stype)) : NULL;
    view->ndim = 2;
    view->shape = REQ_ND(flags) ? info : NULL;
    info[0] = 1;
    info[1] = (Py_ssize_t) self->ref->nrows;
    view->strides = REQ_STRIDES(flags) ? info + 2 : NULL;
    info[2] = view->len;
    info[3] = view->itemsize;
    view->suboffsets = NULL;
    view->internal = (void*) 2;
    column_incref(col);
    return 0;
    fail:
        view->obj = NULL;
        return -1;
}

static int dt_getbuffer(DataTable_PyObject *self, Py_buffer *view, int flags)
{
    Py_ssize_t *info = NULL;
    DataTable *dt = self->ref;
    size_t ncols = (size_t) dt->ncols;
    size_t nrows = (size_t) dt->nrows;

    if (ncols == 0) {
        return dt_getbuffer_no_cols(self, view, flags);
    }

    // Check whether we have a single-column DataTable that doesn't need to be
    // copied -- in which case it should be possible to return the buffer
    // by-reference instead of copying the data into an intermediate buffer.
    if (ncols == 1 && !dt->rowindex && !REQ_WRITABLE(flags) &&
        !stype_info[self->ref->columns[0]->stype].varwidth) {
        return dt_getbuffer_1_col(self, view, flags);
    }

    // Multiple columns datatable => copy all data into a new buffer before
    // passing it to the requester. This is of course very unfortunate, but
    // Numpy (the primary consumer of the buffer protocol) is unable to handle
    // "INDIRECT" buffer.

    // First, find the common stype for all columns in the DataTable.
    SType stype = ST_VOID;
    int64_t stypes_mask = 0;
    for (size_t i = 0; i < ncols; i++) {
        if (!(stypes_mask & (1 << dt->columns[i]->stype))) {
            stypes_mask |= 1 << dt->columns[i]->stype;
            stype = common_stype_for_buffer(stype, dt->columns[i]->stype);
        }
    }

    // Allocate the final buffer
    assert(!stype_info[stype].varwidth);
    size_t elemsize = stype_info[stype].elemsize;
    size_t colsize = nrows * elemsize;
    void *__restrict__ buf = NULL;
    dtmalloc_g(buf, void, ncols * colsize);

    // Construct the data buffer
    for (size_t i = 0; i < ncols; i++) {
        Column *col = dt->columns[i];
        if (dt->rowindex) {
            col = column_extract(col, dt->rowindex);
        }
        if (col->stype == stype) {
            assert(col->alloc_size == colsize);
            memcpy(add_ptr(buf, i * colsize), col->data, colsize);
        } else {
            Column *newcol = column_cast(col, stype);
            if (newcol == NULL) { printf("Cannot cast column %d into %d\n", col->stype, stype); goto fail; }
            assert(newcol->alloc_size == colsize);
            memcpy(add_ptr(buf, i * colsize), newcol->data, colsize);
            column_decref(newcol);
        }
        if (dt->rowindex) {
            column_decref(col);
        }
    }

    // Fill in the `view` struct
    if (REQ_ND(flags)) dtmalloc_g(info, Py_ssize_t, 4);
    view->buf = buf;
    view->obj = (PyObject*) self;
    view->len = (Py_ssize_t)(ncols * colsize);
    view->readonly = 0;
    view->itemsize = (Py_ssize_t) elemsize;
    view->format = REQ_FORMAT(flags) ?
        const_cast<char*>(format_from_stype(stype)) : NULL;
    view->ndim = 2;
    view->shape = REQ_ND(flags)? info : NULL;
    info[0] = (Py_ssize_t) ncols;
    info[1] = (Py_ssize_t) nrows;
    view->strides = REQ_STRIDES(flags)? info + 2 : NULL;
    info[2] = (Py_ssize_t) colsize;
    info[3] = (Py_ssize_t) elemsize;
    view->suboffsets = NULL;
    view->internal = (void*) 3;
    Py_INCREF(self);
    return 0;

    fail:
    view->obj = NULL;
    return -1;
}


/*
    There is a bug in Numpy where they request for an "indirect" buffer but then
    do not know how to handle it... In order to circumvent that we will always
    create "strides" buffer.
    https://github.com/numpy/numpy/issues/9456

    if (REQ_INDIRECT(flags)) {
        size_t elemsize = stype_info[stype].elemsize;
        Py_ssize_t *info = NULL;
        dtmalloc_g(info, Py_ssize_t, 6);
        void **buf = NULL;
        dtmalloc_g(buf, void*, ncols);
        for (int i = 0; i < ncols; i++) {
            column_incref(dt->columns[i]);
            buf[i] = dt->columns[i]->data;
        }

        view->buf = (void*) buf;
        view->obj = (PyObject*) self;
        view->len = ncols * dt->nrows * elemsize;
        view->readonly = 1;
        view->itemsize = elemsize;
        view->format = REQ_FORMAT(flags)? format_from_stype(stype) : NULL;
        view->ndim = 2;
        info[0] = ncols;
        info[1] = dt->nrows;
        view->shape = info;
        info[2] = sizeof(void*);
        info[3] = elemsize;
        view->strides = info + 2;
        info[4] = 0;
        info[5] = -1;
        view->suboffsets = info + 4;
        view->internal = (void*) 1;

        Py_INCREF(self);
        return 0;
    }
*/

static void dt_releasebuffer(DataTable_PyObject *self, Py_buffer *view)
{
    // 1 = 0-col DataTable, 2 = 1-col DataTable, 3 = 2+-col DataTable
    size_t kind = (size_t) view->internal;
    if (kind == 2) {
        column_decref(self->ref->columns[0]);
    }
    if (kind == 3) {
        dtfree(view->buf);
    }
    dtfree(view->shape);
}


PyBufferProcs datatable_as_buffer = {
    (getbufferproc) dt_getbuffer,
    (releasebufferproc) dt_releasebuffer,
};



//==============================================================================
// Buffers protocol for Python-side DataTable object
//==============================================================================

static int pydatatable_getbuffer(PyObject *self, Py_buffer *view, int flags)
{
    PyObject *pydt = PyObject_GetAttrString(self, "internal");
    if (pydt == NULL) {
        PyErr_Format(PyExc_AttributeError,
                     "Cannot retrieve attribute internal");
        return -1;
    }
    return dt_getbuffer((DataTable_PyObject*)pydt, view, flags);
}

static void pydatatable_releasebuffer(UU, Py_buffer *view)
{
    dt_releasebuffer(NULL, view);
}

static PyBufferProcs pydatatable_as_buffer = {
    (getbufferproc) pydatatable_getbuffer,
    (releasebufferproc) pydatatable_releasebuffer,
};

PyObject* pyinstall_buffer_hooks(UU, PyObject *args)
{
    PyObject *obj = NULL;
    if (!PyArg_ParseTuple(args, "O", &obj)) return NULL;
    obj->ob_type->tp_as_buffer = &pydatatable_as_buffer;
    return none();
}



//==============================================================================
// Buffers utility functions
//==============================================================================

static SType stype_from_format(const char *format, int64_t itemsize)
{
    SType stype = ST_VOID;
    char c = format[0];
    if (c == '@' || c == '=') c = format[1];

    if (c == 'b' || c == 'h' || c == 'i' || c == 'l' || c == 'q' || c == 'n') {
        // These are all various integer types
        stype = itemsize == 1 ? ST_INTEGER_I1 :
                itemsize == 2 ? ST_INTEGER_I2 :
                itemsize == 4 ? ST_INTEGER_I4 :
                itemsize == 8 ? ST_INTEGER_I8 : ST_VOID;
    }
    else if (c == 'd' || c == 'f') {
        stype = itemsize == 4 ? ST_REAL_F4 :
                itemsize == 8 ? ST_REAL_F8 : ST_VOID;
    }
    else if (c == '?') {
        stype = itemsize == 1 ? ST_BOOLEAN_I1 : ST_VOID;
    }
    else if (c == 'O') {
        stype = ST_OBJECT_PYPTR;
    }
    if (stype == ST_VOID) {
        PyErr_Format(PyExc_ValueError,
                     "Unknown format '%s' with itemsize %zd", format, itemsize);
    }
    return stype;
}
