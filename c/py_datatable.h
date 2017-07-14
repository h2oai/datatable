#ifndef dt_PY_DATATABLE_H
#define dt_PY_DATATABLE_H
#include <Python.h>
#include "datatable.h"


/**
 * Pythonic "handle" to a DataTable object.
 *
 * This object provides an interface through which Python can access the
 * underlying C DataTable (without encumbering the latter with Python-specific
 * elements).
 *
 * :param ref:
 *     Reference to the C `DataTable` object. The referenced object *is owned*,
 *     that is current object is responsible for deallocating the referenced
 *     datatable upon being garbage-collected.
 *
 */
typedef struct DataTable_PyObject {
    PyObject_HEAD
    DataTable *ref;
} DataTable_PyObject;


extern PyTypeObject DataTable_PyType;




// Exported methods
int dt_unwrap(PyObject *object, void *address);
PyObject* pydt_from_dt(DataTable *dt);
PyObject* pydatatable_from_list(PyObject *self, PyObject *args);
PyObject* pydatatable_assemble(PyObject *self, PyObject *args);
PyObject* pydatatable_load(PyObject *self, PyObject *args);
int init_py_datatable(PyObject *module);

#endif
