#include "sort.h"
#include "py_column.h"
#include "py_types.h"
#include "py_utils.h"



Column_PyObject* pycolumn_from_column(Column *col, DataTable_PyObject *pydt,
                                      int64_t colidx)
{
    Column_PyObject *pycol = Column_PyNew();
    if (pycol == NULL) return NULL;
    pycol->ref = col->incref();
    pycol->pydt = pydt;
    pycol->colidx = colidx;
    Py_XINCREF(pydt);
    return pycol;
}


DT_DOCS(mtype, "'Memory' type of the column: data, or memmap")
static PyObject* get_mtype(Column_PyObject *self) {
    return self->ref->mbuf_repr();
}


DT_DOCS(stype, "'Storage' type of the column")
static PyObject* get_stype(Column_PyObject *self) {
    return incref(py_stype_names[self->ref->stype()]);
}


DT_DOCS(ltype, "'Logical' type of the column")
static PyObject* get_ltype(Column_PyObject *self) {
    return incref(py_ltype_names[stype_info[self->ref->stype()].ltype]);
}


DT_DOCS(data_size, "The amount of memory taken by column's data")
static PyObject* get_data_size(Column_PyObject *self) {
    Column *col = self->ref;
    return PyLong_FromSize_t(col->alloc_size());
}

DT_DOCS(data_pointer, "Pointer (cast to long int) to the column's internal memory buffer")
static PyObject* get_data_pointer(Column_PyObject *self) {
    Column *col = self->ref;
    return PyLong_FromSize_t(reinterpret_cast<size_t>(col->data()));
}


DT_DOCS(meta, "String representation of the column's `meta` struct")
static PyObject* get_meta(Column_PyObject *self) {
    Column *col = self->ref;
    void *meta = col->meta;
    switch (col->stype()) {
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


DT_DOCS(refcount, "Reference count of the column")
static PyObject* get_refcount(Column_PyObject *self) {
    // Subtract 1 from refcount, because this Column_PyObject holds one
    // reference to the column.
    return PyLong_FromLong(self->ref->refcount - 1);
}


DT_DOCS(save_to_disk, "")
static PyObject* meth_save_to_disk(Column_PyObject *self, PyObject *args)
{
    Column *col = self->ref;
    const char *filename = NULL;
    if (!PyArg_ParseTuple(args, "s:save_to_disk", &filename)) return NULL;
    Column *ret = col->save_to_disk(filename);
    if (!ret) return NULL;
    Py_RETURN_NONE;
}


DT_DOCS(hexview, "")
static PyObject* meth_hexview(Column_PyObject *self, UU)
{
    if (pyfn_column_hexview == NULL) {
        PyErr_Format(PyExc_RuntimeError,
                     "Function column_hexview() was not linked");
        return NULL;
    }
    PyObject *v = Py_BuildValue("(OOi)", self, self->pydt, self->colidx);
    PyObject *ret = PyObject_CallObject(pyfn_column_hexview, v);
    Py_XDECREF(v);
    return ret;
}


static void pycolumn_dealloc(Column_PyObject *self)
{
    self->ref->decref();
    Py_XDECREF(self->pydt);
    self->ref = NULL;
    self->pydt = NULL;
    Py_TYPE(self)->tp_free((PyObject*)self);
}



//==============================================================================
// Column type definition
//==============================================================================

static PyGetSetDef column_getseters[] = {
    DT_GETSETTER(mtype),
    DT_GETSETTER(stype),
    DT_GETSETTER(ltype),
    DT_GETSETTER(data_size),
    DT_GETSETTER(data_pointer),
    DT_GETSETTER(meta),
    DT_GETSETTER(refcount),
    {NULL, NULL, NULL, NULL, NULL}  /* sentinel */
};

static PyMethodDef column_methods[] = {
    DT_METHOD1(save_to_disk),
    DT_METHOD1(hexview),
    {NULL, NULL, 0, NULL}           /* sentinel */
};

PyTypeObject Column_PyType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "_datatable.Column",                /* tp_name */
    sizeof(Column_PyObject),            /* tp_basicsize */
    0,                                  /* tp_itemsize */
    (destructor)pycolumn_dealloc,       /* tp_dealloc */
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
    &column_as_buffer,                  /* tp_as_buffer; see py_buffers.c */
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
    init_sort_functions();

    // Register Column_PyType on the module
    Column_PyType.tp_new = PyType_GenericNew;
    if (PyType_Ready(&Column_PyType) < 0) return 0;
    Py_INCREF(&Column_PyType);
    PyModule_AddObject(module, "Column", (PyObject*) &Column_PyType);

    return 1;
}
