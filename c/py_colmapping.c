#include "py_colmapping.h"
#include "py_datatable.h"


int init_py_colmapping(PyObject *module)
{
    ColMapping_PyType.tp_new = PyType_GenericNew;
    if (PyType_Ready(&ColMapping_PyType) < 0) return 0;
    Py_INCREF(&ColMapping_PyType);
    PyModule_AddObject(module, "ColMapping", (PyObject*) &ColMapping_PyType);
    return 1;
}


ColMapping_PyObject* ColMappingPy_from_array(PyObject *self, PyObject *args)
{
    PyObject *list;
    DataTable_PyObject *dt;
    ColMapping *colmapping = NULL;
    ColMapping_PyObject *res = NULL;
    int64_t *data = NULL;

    // Unpack arguments and check their validity
    if (!PyArg_ParseTuple(args, "O!O!:ColMapping.from_array",
                          &PyList_Type, &list, &DataTable_PyType, &dt))
        return NULL;

    // Convert Pythonic List into a regular C array of longs
    int64_t len = (int64_t) PyList_Size(list);
    data = malloc(sizeof(int64_t) * len);
    if (data == NULL) goto fail;
    for (int64_t i = 0; i < len; ++i) {
        data[i] = PyLong_AsLong(PyList_GET_ITEM(list, i));
    }

    // Construct and return the ColMapping object
    colmapping = ColMapping_from_array(data, len, dt->ref);
    res = ColMapping_PyNEW();
    data = NULL;
    if (res == NULL || colmapping == NULL) goto fail;
    res->ref = colmapping;
    return res;

  fail:
    free(data);
    ColMapping_dealloc(colmapping);
    Py_XDECREF(res);
    return NULL;
}



//------ ColMapping PyObject -----------------------------------------------------

static void __dealloc__(ColMapping_PyObject *self)
{
    ColMapping_dealloc(self->ref);
    Py_TYPE(self)->tp_free((PyObject*)self);
}


PyTypeObject ColMapping_PyType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "_datatable.ColMapping",            /* tp_name */
    sizeof(ColMapping_PyObject),        /* tp_basicsize */
    0,                                  /* tp_itemsize */
    (destructor)__dealloc__,            /* tp_dealloc */
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
};
