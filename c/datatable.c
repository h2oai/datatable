#include "datatable.h"
#include "structmember.h"
#include "limits.h"


/* should be moved to utilities I guess... */
static PyObject* none() {
    PyObject* x = Py_None;
    Py_INCREF(x);
    return x;
}


static void dt_Datatable_dealloc(dt_DatatableObject* self)
{
    if (self->ncols > 0) {
        for (int i = self->ncols - 1; i >= 0; --i) {
            dt_Coltype ctype = self->coltypes[i];
            dt_Coldata cdata = self->columns[i];
            switch (ctype) {
                case DT_DOUBLE:
                    free(cdata.ddouble);
                    break;
                case DT_LONG:
                    free(cdata.dlong);
                    break;
                case DT_STRING:
                    for (int j = self->nrows - 1; j >= 0; --j) {
                        free(cdata.dstring[j]);
                    }
                    free(cdata.dstring);
                    break;
                case DT_BOOL:
                    free(cdata.dbool);
                    break;
                case DT_OBJECT:
                    for (int j = self->nrows - 1; j >= 0; --j) {
                        Py_XDECREF(cdata.dobject[j]);
                    }
                    free(cdata.dobject);
                    break;
            }
        }
        free(self->coltypes);
        free(self->columns);
    }
    Py_TYPE(self)->tp_free((PyObject*)self);
}

static PyObject* dt_Datatable_window(dt_DatatableObject* self, PyObject* args)
{
    int col0, ncols, nrows;
    long row0;

    if (!PyArg_ParseTuple(args, "iili", &col0, &ncols, &row0, &nrows))
        return NULL;

    dt_DtWindowObject *res = (dt_DtWindowObject*)
            PyObject_CallObject((PyObject*) &dt_DtWindowType, NULL);
    if (res == NULL) return NULL;

    res->col0 = col0;
    res->ncols = ncols;
    res->row0 = row0;
    res->nrows = nrows;

    Py_XDECREF(res->data);
    res->data = PyList_New((Py_ssize_t) ncols);
    for (Py_ssize_t i = 0; i < ncols; ++i) {
        dt_Coltype coltype = self->coltypes[col0 + i];
        dt_Coldata coldata = self->columns[col0 + i];
        PyObject *collist = PyList_New((Py_ssize_t) nrows);
        for (Py_ssize_t j = 0; j < nrows; ++j) {
            PyObject *value;
            switch (coltype) {
                case DT_DOUBLE:
                    value = (PyObject*) PyFloat_FromDouble(coldata.ddouble[row0 + nrows]);
                    break;

                case DT_LONG: {
                    long x = coldata.dlong[row0 + nrows];
                    value = x == LONG_MIN? none() : (PyObject*) PyLong_FromLong(x);
                }   break;

                case DT_STRING: {
                    char* x = coldata.dstring[row0 + nrows];
                    value = x == NULL? none() : (PyObject*) PyUnicode_FromString(x);
                }   break;

                case DT_BOOL: {
                    unsigned char x = coldata.dbool[row0 + nrows];
                    value = x == 0? Py_False : x == 1? Py_True : Py_None;
                    Py_INCREF(value);
                }   break;

                case DT_OBJECT:
                    value = coldata.dobject[row0 + nrows];
                    Py_INCREF(value);
                    break;
            }
            PyList_SetItem(collist, j, value);
        }
        PyList_SetItem(res->data, i, (PyObject*)collist);
    }

    return (PyObject*) res;
}


PyDoc_STRVAR(dtdoc_window, "Retrieve datatable's data within a window");
PyDoc_STRVAR(dtdoc_ncols, "Number of columns");
PyDoc_STRVAR(dtdoc_col0, "Index of the first column");
PyDoc_STRVAR(dtdoc_nrows, "Number of rows");
PyDoc_STRVAR(dtdoc_row0, "Index of the first row");
PyDoc_STRVAR(dtdoc_windowdata, "Datatable's data within the specified window");


static PyMethodDef dt_Datatable_methods[] = {
    {"window", (PyCFunction)dt_Datatable_window, METH_VARARGS, dtdoc_window},
    {NULL, NULL}           /* sentinel */
};

static PyMemberDef dt_Datatable_members[] = {
    {"ncols", T_INT,  offsetof(dt_DatatableObject, ncols), READONLY, dtdoc_ncols},
    {"nrows", T_LONG, offsetof(dt_DatatableObject, nrows), READONLY, dtdoc_nrows},
    {NULL}                 /* sentinel */
};


PyTypeObject dt_DatatableType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "_datatable.DataTable",             /* tp_name */
    sizeof(dt_DatatableObject),         /* tp_basicsize */
    0,                                  /* tp_itemsize */
    (destructor)dt_Datatable_dealloc,   /* tp_dealloc */
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
    0,                                  /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT,                 /* tp_flags */
    "DataTable object",                 /* tp_doc */
    0,                                  /* tp_traverse */
    0,                                  /* tp_clear */
    0,                                  /* tp_richcompare */
    0,                                  /* tp_weaklistoffset */
    0,                                  /* tp_iter */
    0,                                  /* tp_iternext */
    dt_Datatable_methods,               /* tp_methods */
    dt_Datatable_members,               /* tp_members */
    0,                                  /* tp_getset */
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
};




/*------------------------------------------------------------------------------
 * DtWindow type
 *----------------------------------------------------------------------------*/


static void dt_DtWindow_dealloc(dt_DtWindowObject* self)
{
    Py_XDECREF(self->data);
    Py_TYPE(self)->tp_free((PyObject*)self);
}


static PyMemberDef dt_DtWindow_members[] = {
    {"ncols", T_INT,  offsetof(dt_DtWindowObject, ncols), READONLY, dtdoc_ncols},
    {"nrows", T_INT,  offsetof(dt_DtWindowObject, nrows), READONLY, dtdoc_nrows},
    {"col0",  T_INT,  offsetof(dt_DtWindowObject, col0), READONLY, dtdoc_col0},
    {"row0",  T_LONG, offsetof(dt_DtWindowObject, row0), READONLY, dtdoc_row0},
    {"data",  T_OBJECT_EX, offsetof(dt_DtWindowObject, data), READONLY, dtdoc_windowdata},
    {NULL}                 /* sentinel */
};


PyTypeObject dt_DtWindowType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "_datatable.DataWindow",            /* tp_name */
    sizeof(dt_DtWindowObject),          /* tp_basicsize */
    0,                                  /* tp_itemsize */
    (destructor)dt_DtWindow_dealloc,    /* tp_dealloc */
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
    0,                                  /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT,                 /* tp_flags */
    "DtWindow object",                  /* tp_doc */
    0,                                  /* tp_traverse */
    0,                                  /* tp_clear */
    0,                                  /* tp_richcompare */
    0,                                  /* tp_weaklistoffset */
    0,                                  /* tp_iter */
    0,                                  /* tp_iternext */
    0,                                  /* tp_methods */
    dt_DtWindow_members,                /* tp_members */
};
