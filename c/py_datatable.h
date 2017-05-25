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
 * :param source:
 *     If referenced datatable is a view, then this field should point to the
 *     DataTable_PyObject of the view's parent datatable. The purpose of this
 *     field is to ensure proper lifetime of each datatable.
 *
 *     For example, consider a datatble `dt1` with pythonic handle `pydt1`. Now
 *     the user constructs a view `dt2` (with handle `pydt2`) on `dt1`. Thus
 *     `dt2->source = dt1`. Now suppose `pydt1` gets garbage-collected by
 *     Python and needs to destroy `dt1`. Since `pydt1` doesn't know that there
 *     is a C-level reference to `dt1` somewhere, that object will be freed,
 *     causing `dt2` to become corrupt. However if we keep a reference
 *     `pydt2->source = pydt1`, then this reference will prevent `pydt1` from
 *     being garbage-collected (at least until `pydt2` is alive).
 *
 */
typedef struct DataTable_PyObject {
    PyObject_HEAD
    DataTable *ref;
    struct DataTable_PyObject *source;
} DataTable_PyObject;


extern PyTypeObject DataTable_PyType;




// Exported methods
int dt_unwrap(PyObject *object, void *address);
PyObject* pydt_from_dt(DataTable *dt, DataTable_PyObject *src);
PyObject* pydatatable_from_list(PyTypeObject *type, PyObject *args);
PyObject* write_column_to_file(PyObject *self, PyObject *args);
PyObject* pydatatable_assemble(PyObject *self, PyObject *args);
PyObject* pydatatable_assemble_view(PyObject *self, PyObject *args);
int init_py_datatable(PyObject *module);

#endif
