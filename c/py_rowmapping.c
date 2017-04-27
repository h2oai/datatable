#include "py_datatable.h"
#include "py_rowmapping.h"
#include "datatable.h"


static RowMapping_PyObject* pyrowmapping(RowMapping *src)
{
    if (src == NULL) return NULL;
    RowMapping_PyObject *res = (RowMapping_PyObject*) \
        PyObject_CallObject((PyObject*) &RowMapping_PyType, NULL);
    if (res == NULL) {
        rowmapping_dealloc(src);
        return NULL;
    }
    res->ref = src;
    return res;
}


RowMapping_PyObject*
RowMappingPy_from_slice(PyObject *self, PyObject *args)
{
    int64_t start, count, step;

    if (!PyArg_ParseTuple(args, "LLL:RowMapping.from_slice",
                          &start, &count, &step))
        return NULL;

    if (start < 0 || count < 0) {
        PyErr_SetString(PyExc_ValueError,
                        "`start` and `count` must be nonnegative");
        return NULL;
    }

    return pyrowmapping(rowmapping_from_slice(start, count, step));
}



RowMapping_PyObject*
RowMappingPy_from_slicelist(PyObject *self, PyObject *args)
{
    PyObject *pystarts, *pycounts, *pysteps;
    int64_t *starts = NULL, *counts = NULL, *steps = NULL;
    RowMapping *rowmapping = NULL;
    RowMapping_PyObject *res = NULL;

    // Unpack arguments
    if (!PyArg_ParseTuple(args, "O!O!O!:RowMapping.from_slicelist",
                          &PyList_Type, &pystarts, &PyList_Type, &pycounts,
                          &PyList_Type, &pysteps))
        return NULL;

    int64_t n = (int64_t) PyList_Size(pystarts);
    int64_t n2 = (int64_t) PyList_Size(pycounts);
    int64_t n3 = (int64_t) PyList_Size(pysteps);
    assert (n >= n2 && n >= n3);
    starts = malloc(sizeof(int64_t) * (size_t)n);
    counts = malloc(sizeof(int64_t) * (size_t)n);
    steps = malloc(sizeof(int64_t) * (size_t)n);
    if (starts == NULL || counts == NULL || steps == NULL) goto fail;

    // Convert Pythonic lists into regular C arrays of longs
    for (int64_t i = 0; i < n; i++) {
        starts[i] = PyLong_AsLong(PyList_GET_ITEM(pystarts, i));
        counts[i] = i < n2? PyLong_AsLong(PyList_GET_ITEM(pycounts, i)) : 1;
        steps[i] = i < n3? PyLong_AsLong(PyList_GET_ITEM(pysteps, i)) : 1;
    }

    // Construct and return the RowMapping object
    rowmapping = rowmapping_from_slicelist(starts, counts, steps, n);
    res = RowMapping_PyNEW();
    if (res == NULL || rowmapping == NULL) goto fail;
    res->ref = rowmapping;
    return res;

  fail:
    free(starts);
    free(counts);
    free(steps);
    rowmapping_dealloc(rowmapping);
    return NULL;
}



RowMapping_PyObject* RowMappingPy_from_array(PyObject *self, PyObject *args)
{
    PyObject *list;
    RowMapping *rowmapping = NULL;
    RowMapping_PyObject *res = NULL;
    int64_t *data = NULL;

    // Unpack arguments and check their validity
    if (!PyArg_ParseTuple(args, "O!:RowMapping.from_array",
                          &PyList_Type, &list))
        return NULL;

    // Convert Pythonic List into a regular C array of longs
    int64_t len = (int64_t) PyList_Size(list);
    data = malloc(sizeof(int64_t) * (size_t)len);
    if (data == NULL) goto fail;
    for (int64_t i = 0; i < len; ++i) {
        data[i] = PyLong_AsLong(PyList_GET_ITEM(list, i));
    }

    // Construct and return the RowMapping object
    rowmapping = rowmapping_from_i64_array(data, len);
    res = RowMapping_PyNEW();
    if (res == NULL || rowmapping == NULL) goto fail;
    res->ref = rowmapping;
    return res;

  fail:
    free(data);
    rowmapping_dealloc(rowmapping);
    Py_XDECREF(res);
    return NULL;
}



RowMapping_PyObject* RowMappingPy_from_column(PyObject *self, PyObject *args)
{
    DataTable_PyObject *pydt = NULL;
    RowMapping *rowmapping = NULL;
    RowMapping_PyObject *res = NULL;

    if (!PyArg_ParseTuple(args, "O!:RowMapping.from_column",
                          &DataTable_PyType, &pydt))
        return NULL;

    rowmapping = RowMapping_from_column(pydt->ref);
    res = RowMapping_PyNEW();
    if (res == NULL || rowmapping == NULL) goto fail;
    res->ref = rowmapping;
    return res;

  fail:
    rowmapping_dealloc(rowmapping);
    Py_XDECREF(res);
    return NULL;
}


RowMapping_PyObject* RowMappingPy_from_RowMapping(RowMapping* rowmapping)
{
    if (rowmapping == NULL) return NULL;
    RowMapping_PyObject *res = RowMapping_PyNEW();
    if (res == NULL) return NULL;
    res->ref = rowmapping;
    return res;
}


//==============================================================================
//  RowMapping PyObject
//==============================================================================

static void __dealloc__(RowMapping_PyObject *self)
{
    rowmapping_dealloc(self->ref);
    Py_TYPE(self)->tp_free((PyObject*)self);
}


static PyObject* __repr__(RowMapping_PyObject *self)
{
    RowMapping *rwm = self->ref;
    if (rwm == NULL)
        return PyUnicode_FromString("_RowMapping(NULL)");
    if (rwm->type == RM_ARR32) {
        return PyUnicode_FromFormat("_RowMapping(int32[%ld])", rwm->length);
    }
    if (rwm->type == RM_ARR64) {
        return PyUnicode_FromFormat("_RowMapping(int64[%ld])", rwm->length);
    }
    if (rwm->type == RM_SLICE) {
        return PyUnicode_FromFormat("_RowMapping(%ld:%ld:%ld)",
            rwm->slice.start, rwm->length, rwm->slice.step);
    }
    return NULL;
}


PyTypeObject RowMapping_PyType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "_datatable.RowMapping",            /* tp_name */
    sizeof(RowMapping_PyObject),        /* tp_basicsize */
    0,                                  /* tp_itemsize */
    (destructor)__dealloc__,            /* tp_dealloc */
    0,                                  /* tp_print */
    0,                                  /* tp_getattr */
    0,                                  /* tp_setattr */
    0,                                  /* tp_compare */
    (reprfunc)__repr__,                 /* tp_repr */
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
    0,0,0,0,0,0,0,0,0,0,0,0
};


// Add PyRowMapping object to the Python module
int init_py_rowmapping(PyObject *module)
{
    RowMapping_PyType.tp_new = PyType_GenericNew;
    if (PyType_Ready(&RowMapping_PyType) < 0) return 0;
    Py_INCREF(&RowMapping_PyType);
    PyModule_AddObject(module, "RowMapping", (PyObject*) &RowMapping_PyType);
    return 1;
}
