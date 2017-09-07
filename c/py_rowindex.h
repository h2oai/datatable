#ifndef dt_PY_ROWINDEX_H
#define dt_PY_ROWINDEX_H
#include <Python.h>
#include "rowindex.h"


/**
 * Pythonic reference to a `RowIndex` object.
 */
typedef struct RowIndex_PyObject {
    PyObject_HEAD
    RowIndex *ref;
} RowIndex_PyObject;

extern PyTypeObject RowIndex_PyType;



PyObject* pyrowindex(RowIndex *src);
int rowindex_unwrap(PyObject *object, void *address);
PyObject* pyrowindex_from_slice(PyObject*, PyObject *args);
PyObject* pyrowindex_from_slicelist(PyObject*, PyObject *args);
PyObject* pyrowindex_from_array(PyObject*, PyObject *args);
PyObject* pyrowindex_from_boolcolumn(PyObject*, PyObject *args);
PyObject* pyrowindex_from_intcolumn(PyObject*, PyObject *args);
PyObject* pyrowindex_from_filterfn(PyObject*, PyObject *args);
PyObject* pyrowindex_from_function(PyObject*, PyObject *args);
PyObject* pyrowindex_uplift(PyObject*, PyObject *args);

int init_py_rowindex(PyObject *module);

#endif
