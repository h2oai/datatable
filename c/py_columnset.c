#include "columnset.h"
#include "py_columnset.h"
#include "py_utils.h"
#include "utils.h"


static PyObject* py(Column **columns)
{
    if (columns == NULL) return NULL;
    PyObject *res = PyObject_CallObject((PyObject*) &ColumnSet_PyType, NULL);
    ((ColumnSet_PyObject*) res)->columns = columns;
    return res;
}



PyObject* pycolumns_from_pymixed(PyObject *self, PyObject *args)
{
    PyObject *elems;
    if (!PyArg_ParseTuple(args, "O!", &PyList_Type, &elems))
        return NULL;
    return py(columns_from_pymixed(elems, NULL, NULL, NULL));
}



Column** columns_from_pymixed(
    PyObject *elems,
    DataTable *dt,
    RowMapping *rowmapping,
    int (*mapfn)(int64_t row0, int64_t row1, void** out)
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
    return columns_from_mixed(spec, ncols, dt, rowmapping, mapfn);

  fail:
    return NULL;
}



//======================================================================================================================
// PyRef type definition
//======================================================================================================================

static void dealloc(ColumnSet_PyObject *self)
{
    Column **ptr = self->columns;
    while (*ptr) {
        free(*ptr);
        ptr++;
    }
    free(self->columns);
    Py_TYPE(self)->tp_free((PyObject*)self);
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
