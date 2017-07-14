#include "columnset.h"
#include "py_columnset.h"
#include "py_datatable.h"
#include "py_rowindex.h"
#include "py_utils.h"
#include "utils.h"


static PyObject* py(Column **columns)
{
    if (columns == NULL) return NULL;
    PyObject *res = PyObject_CallObject((PyObject*) &ColumnSet_PyType, NULL);
    if (res == NULL) return NULL;
    ((ColumnSet_PyObject*) res)->columns = columns;
    return res;
}

/**
 * Helper function to be used with `PyArg_ParseTuple()` in order to extract
 * a `Column**` pointer out of the arguments tuple. Usage:
 *
 *     Column **columns;
 *     if (!PyArg_ParseTuple(args, "O&", &columnset_unwrap, &columns))
 *         return NULL;
 */
int columnset_unwrap(PyObject *object, void *address) {
    Column ***ans = address;
    if (!PyObject_TypeCheck(object, &ColumnSet_PyType)) {
        PyErr_SetString(PyExc_TypeError,
                        "Expected argument of type ColumnSet");
        return 0;
    }
    *ans = ((ColumnSet_PyObject*)object)->columns;
    return 1;
}


//==============================================================================

PyObject* pycolumns_from_slice(UU, PyObject *args)
{
    DataTable *dt;
    int64_t start, count, step;
    if (!PyArg_ParseTuple(args, "O&LLL:columns_from_slice",
                          &dt_unwrap, &dt, &start, &count, &step))
        return NULL;

    PyObject* res = py(columns_from_slice(dt, start, count, step));
    return res;
}



PyObject* pycolumns_from_pymixed(UU, PyObject *args)
{
    PyObject *elems;
    DataTable *dt;
    RowIndex *ri;
    long long rawptr;
    if (!PyArg_ParseTuple(args, "O!O&O&L:columns_from_pymixed",
                          &PyList_Type, &elems,
                          &dt_unwrap, &dt,
                          &rowindex_unwrap, &ri,
                          &rawptr))
        return NULL;

    columnset_mapfn *fnptr = (columnset_mapfn*) rawptr;
    return py(columns_from_pymixed(elems, dt, ri, fnptr));
}



Column** columns_from_pymixed(
    PyObject *elems,
    DataTable *dt,
    RowIndex *rowindex,
    columnset_mapfn *mapfn
) {
    int64_t ncols = PyList_Size(elems);
    int64_t *spec = NULL;

    dtmalloc(spec, int64_t, ncols);
    for (int64_t i = 0; i < ncols; i++) {
        PyObject *elem = PyList_GET_ITEM(elems, i);
        if (PyLong_CheckExact(elem)) {
            spec[i] = (int64_t) PyLong_AsSize_t(elem);
        } else {
            spec[i] = -TOINT64(ATTR(elem, "itype"), 0);
        }
    }
    return columns_from_mixed(spec, ncols, dt, rowindex, mapfn);

  fail:
    return NULL;
}



//======================================================================================================================
// PyRef type definition
//======================================================================================================================

static void dealloc(ColumnSet_PyObject *self)
{
    Column **ptr = self->columns;
    if (ptr) {
        while (*ptr) {
            free(*ptr);
            ptr++;
        }
        free(self->columns);
    }
    Py_TYPE(self)->tp_free((PyObject*)self);
}


static PyObject* repr(ColumnSet_PyObject *self)
{
    Column **ptr = self->columns;
    if (ptr == NULL)
        return PyUnicode_FromString("_ColumnSet(NULL)");
    int ncols = 0;
    while (ptr[ncols]) ncols++;
    return PyUnicode_FromFormat("_ColumnSet(ncols=%d)", ncols);
}



PyTypeObject ColumnSet_PyType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "_datatable.ColumnSet",             /* tp_name */
    sizeof(ColumnSet_PyObject),         /* tp_basicsize */
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


// Add PyColumnSet object to the Python module
int init_py_columnset(PyObject *module)
{
    ColumnSet_PyType.tp_new = PyType_GenericNew;
    if (PyType_Ready(&ColumnSet_PyType) < 0) return 0;
    Py_INCREF(&ColumnSet_PyType);
    PyModule_AddObject(module, "ColumnSet", (PyObject*) &ColumnSet_PyType);
    return 1;
}
