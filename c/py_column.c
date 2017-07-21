#include "py_column.h"
#include "py_types.h"
#include "py_utils.h"

static PyObject* py_rowindextypes[4];


Column_PyObject* pyColumn_from_Column(Column *col)
{
    Column_PyObject *pycol = Column_PyNew();
    if (pycol == NULL) return NULL;
    pycol->ref = col;
    return pycol;
}


static PyObject* get_mtype(Column_PyObject *self) {
    return incref(py_rowindextypes[self->ref->mtype]);
}


static PyObject* get_stype(Column_PyObject *self) {
    return incref(py_stype_names[self->ref->stype]);
}


static PyObject* get_ltype(Column_PyObject *self) {
    return incref(py_ltype_names[stype_info[self->ref->stype].ltype]);
}


static PyObject* get_data_size(Column_PyObject *self) {
    Column *col = self->ref;
    return PyLong_FromSize_t(col->alloc_size);
}


static PyObject* get_meta(Column_PyObject *self) {
    Column *col = self->ref;
    void *meta = col->meta;
    switch (col->stype) {
        case ST_STRING_I4_VCHAR:
        case ST_STRING_I8_VCHAR:
            return PyUnicode_FromFormat("offoff=%lld",
                                        ((VarcharMeta*)meta)->offoff);
        case ST_STRING_FCHAR:
            return PyUnicode_FromFormat("n=%d", ((FixcharMeta*)meta)->n);
        case ST_REAL_I2:
        case ST_REAL_I4:
        case ST_REAL_I8:
            return PyUnicode_FromFormat("scale=%d",
                                        ((DecimalMeta*)meta)->scale);
        case ST_STRING_U1_ENUM:
        case ST_STRING_U2_ENUM:
        case ST_STRING_U4_ENUM: {
            EnumMeta *m = (EnumMeta*) meta;
            return PyUnicode_FromFormat("offoff=%lld&dataoff=%lld&nlevels=%d",
                                        m->offoff, m->dataoff, m->nlevels);
        }
        default:
            if (meta == NULL)
                return none();
            else
                return PyUnicode_FromFormat("%p", meta);
    }
}


static PyObject* get_refcount(Column_PyObject *self) {
    return PyLong_FromLong(self->ref->refcount);
}


static PyObject* pycolumn_save_to_disk(Column_PyObject *self, PyObject *args)
{
    Column *col = self->ref;
    const char *filename = NULL;
    if (!PyArg_ParseTuple(args, "s:save_to_disk", &filename)) return NULL;
    Column *ret = column_save_to_disk(col, filename);
    if (!ret) return NULL;
    Py_RETURN_NONE;
}



/**
 * Provide Python Buffers interface (PEP 3118). This is mostly for the benefit
 * of integrating with Numpy, however in theory other packages can make use of
 * this as well.
 */
static int getbuffer(Column_PyObject *self, Py_buffer *view, int flags)
{
    Column *col = self->ref;
    Py_ssize_t *info = NULL;
    dtmalloc_g(info, Py_ssize_t, 2);

    if (flags & PyBUF_WRITABLE) {
        PyErr_SetString(PyExc_BufferError, "Cannot create writable buffer");
        goto fail;
    }
    if (stype_info[col->stype].varwidth) {
        PyErr_SetString(PyExc_BufferError, "Column's data has variable width");
        goto fail;
    }

    size_t elemsize = stype_info[col->stype].elemsize;

    view->buf = col->data;
    view->obj = (PyObject*) self;
    view->len = (Py_ssize_t) col->alloc_size;
    view->itemsize = (Py_ssize_t) elemsize;
    view->readonly = 0;
    view->ndim = 1;
    view->shape = info;
    view->strides = info + 1;
    view->internal = NULL;
    info[0] = view->len / view->itemsize;
    info[1] = view->itemsize;
    if (flags & PyBUF_FORMAT) {
        view->format = col->stype == ST_BOOLEAN_I1? "?" :
                       col->stype == ST_INTEGER_I1? "b" :
                       col->stype == ST_INTEGER_I2? "h" :
                       col->stype == ST_INTEGER_I4? "i" :
                       col->stype == ST_INTEGER_I8? "q" :
                       col->stype == ST_REAL_F4? "f" :
                       col->stype == ST_REAL_F8? "d" : "x";
    } else {
        view->format = NULL;
    }

    Py_INCREF(self);
    column_incref(col);
    return 0;

  fail:
    view->obj = NULL;
    dtfree(info);
    return -1;
}


static void releasebuffer(Column_PyObject *self, Py_buffer *view)
{
    dtfree(view->shape);
    column_decref(self->ref);
}


void free_xbuf_column(Column *col)
{
    if (col->mtype == MT_XBUF)
        PyBuffer_Release(col->pybuf);
}


//==============================================================================
// Column type definition
//==============================================================================

PyDoc_STRVAR(dtdoc_ltype, "'Logical' type of the column");
PyDoc_STRVAR(dtdoc_stype, "'Storage' type of the column");
PyDoc_STRVAR(dtdoc_mtype, "'Memory' type of the column: data, or memmap");
PyDoc_STRVAR(dtdoc_data_size, "The amount of memory taken by column's data");
PyDoc_STRVAR(dtdoc_meta, "String representation of the column's `meta` struct");
PyDoc_STRVAR(dtdoc_refcount, "Reference count of the column");

#define GETSET1(name) {#name, (getter)get_##name, NULL, dtdoc_##name, NULL}
static PyGetSetDef column_getseters[] = {
    GETSET1(mtype),
    GETSET1(stype),
    GETSET1(ltype),
    GETSET1(data_size),
    GETSET1(meta),
    GETSET1(refcount),
    {NULL, NULL, NULL, NULL, NULL}  /* sentinel */
};
#undef GETSET1

#define METHOD1(name) {#name, (PyCFunction)pycolumn_##name, METH_VARARGS, NULL}
static PyMethodDef column_methods[] = {
    METHOD1(save_to_disk),
    {NULL, NULL, 0, NULL}           /* sentinel */
};
#undef METHOD1

static PyBufferProcs column_as_buffer = {
    (getbufferproc) getbuffer,
    (releasebufferproc) releasebuffer,
};

PyTypeObject Column_PyType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "_datatable.Column",                /* tp_name */
    sizeof(Column_PyObject),            /* tp_basicsize */
    0,                                  /* tp_itemsize */
    0,                                  /* tp_dealloc */
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
    &column_as_buffer,                  /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT,                 /* tp_flags */
    "Column object",                    /* tp_doc */
    0,                                  /* tp_traverse */
    0,                                  /* tp_clear */
    0,                                  /* tp_richcompare */
    0,                                  /* tp_weaklistoffset */
    0,                                  /* tp_iter */
    0,                                  /* tp_iternext */
    column_methods,                     /* tp_methods */
    0,                                  /* tp_members */
    column_getseters,                   /* tp_getset */
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
    0,                                  /* tp_finalize */
};


int init_py_column(PyObject *module) {
    init_column_cast_functions();

    // Register Column_PyType on the module
    Column_PyType.tp_new = PyType_GenericNew;
    if (PyType_Ready(&Column_PyType) < 0) return 0;
    Py_INCREF(&Column_PyType);
    PyModule_AddObject(module, "Column", (PyObject*) &Column_PyType);

    py_rowindextypes[0] = NULL;
    py_rowindextypes[MT_DATA] = PyUnicode_FromString("data");
    py_rowindextypes[MT_MMAP] = PyUnicode_FromString("mmap");
    py_rowindextypes[MT_TEMP] = PyUnicode_FromString("temp");
    py_rowindextypes[MT_XBUF] = PyUnicode_FromString("xbuf");
    return 1;
}
