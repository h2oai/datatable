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
    SType use_stype_for_buffers;
    int64_t : 56;
} DataTable_PyObject;


extern PyBufferProcs datatable_as_buffer;
extern PyTypeObject DataTable_PyType;




// Exported methods
int dt_unwrap(PyObject *object, DataTable **address);
PyObject* pydt_from_dt(DataTable *dt);
PyObject* pydatatable_from_list(PyObject *self, PyObject *args);
PyObject* pydatatable_from_buffers(PyObject *self, PyObject *args);
PyObject* pydatatable_assemble(PyObject *self, PyObject *args);
PyObject* pydatatable_load(PyObject *self, PyObject *args);
PyObject* pyinstall_buffer_hooks(PyObject *self, PyObject *args);
int init_py_datatable(PyObject *module);

DataTable* datatable_unwrapx(PyObject *object);

#endif
