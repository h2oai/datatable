#ifndef dt_PY_DATAWINDOW_H
#define dt_PY_DATAWINDOW_H
#include <Python.h>


/**
 * This object facilitates access to a `DataTable`s data from Python. The idea
 * is that a datatable may be huge, possibly containing gigabytes of data. At
 * the same time, exposing any primitive to a Python runtime requires wrapping
 * that primitive into a `PyObject`, which adds a significant amount of
 * overhead (both in terms of memory and CPU).
 *
 * `DataWindow` objects come to the rescue: they take small rectangular
 * subsections of a datatable's data, and represent them as Python objects.
 * Such limited amount of data is usually sufficient from users' perspective
 * since they are able to view only a limited amount of data in their viewport
 * anyways.
 */
typedef struct DataWindow_PyObject {
    PyObject_HEAD

    // Coordinates of the window returned: `row0`..`row1` x `col0`..`col1`.
    // `row0` is the first row to include, `row1` is 1 after the last. The
    // number of rows in the window is thus `row1 - row0`. Similarly with cols.
    int64_t row0, row1;
    int64_t col0, col1;

    // List of types (ColType) of each column returned. This list will have
    // `col1 - col0` elements.
    PyListObject *types;

    // Actual data within the window, represented as a PyList of PyLists of
    // Python primitives (such as PyLong, PyFloat, etc). The data is returned
    // in column-major order, i.e. each element of the list `data` represents
    // a single column from the parent datatable. The number of elements in
    // this list is `col1 - col0`; and each element is a list of `row1 - row0`
    // items.
    PyListObject *data;

} DataWindow_PyObject;


/** Python type corresponding to DataWindow_PyObject. */
PyTypeObject DataWindow_PyType;


#endif
