#include <Python.h>
#include "datatable.h"
#include "dtutils.h"


static PyObject* dumpster(PyObject *self, PyObject *args) {
    PyObject *arg;
    if (!PyArg_ParseTuple(args, "O", &arg))
        return NULL;

    long s = _PyObject_VAR_SIZE(Py_TYPE(arg), Py_SIZE(arg));
    printf("VAR_SIZE = %ld\n", s);
    printf("Py_SIZE = %ld\n", Py_SIZE(arg));
    printf("tp_basicsize = %ld\n", Py_TYPE(arg)->tp_basicsize);
    printf("tp_itemsize = %ld\n", Py_TYPE(arg)->tp_itemsize);
    printf("tp_name = %s\n", Py_TYPE(arg)->tp_name);
    char *buf = malloc(sizeof(char) * s * 3 + 1);
    char *ptr = (char*) (void*) arg;
    const char *hex = "0123456789ABCDEF";
    for (int i = 0; i < s; i++) {
        char ch = ptr[i];
        buf[i*3 + 0] = hex[(ch & 0xF0) >> 4];
        buf[i*3 + 1] = hex[ch & 0x0F];
        buf[i*3 + 2] = ' ';
    }
    buf[s*3] = 0;
    return PyUnicode_FromStringAndSize(buf, s*3);
}



//------------------------------------------------------------------------------
// Module definition
//------------------------------------------------------------------------------

static PyMethodDef DatatableModuleMethods[] = {
    {"dumpster", dumpster, METH_VARARGS, "Just a test function"},

    {NULL, NULL, 0, NULL}  /* Sentinel */
};

static PyModuleDef datatablemodule = {
    PyModuleDef_HEAD_INIT,
    "_datatable",  /* name of the module */
    "module doc",  /* module documentation */
    -1,            /* size of per-interpreter state of the module, or -1
                      if the module keeps state in global variables */
    DatatableModuleMethods
};

/* Called when Python program imports the module */
PyMODINIT_FUNC
PyInit__datatable(void) {
    PyObject *m;

    // Sanity checks
    assert(sizeof(char) == sizeof(unsigned char));

    Py_int0 = PyLong_FromLong(0);
    Py_int1 = PyLong_FromLong(1);

    dt_DatatableType.tp_new = PyType_GenericNew;
    dt_DtViewType.tp_new = PyType_GenericNew;
    if (PyType_Ready(&dt_DatatableType) < 0 ||
        PyType_Ready(&dt_DtViewType) < 0)
        return NULL;

    m = PyModule_Create(&datatablemodule);
    if (m == NULL)
        return NULL;

    Py_INCREF(&dt_DatatableType);
    PyModule_AddObject(m, "DataTable", (PyObject*) &dt_DatatableType);
    PyModule_AddObject(m, "DataView", (PyObject*) &dt_DtViewType);
    return m;
}
