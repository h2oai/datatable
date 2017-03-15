#ifndef dt_DATAWINDOW_H
#define dt_DATAWINDOW_H
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
typedef struct dt_DataWindowObject {
    PyObject_HEAD

    // Coordinates of the window returned: `ncols` columns starting from `col0`
    // and `nrows` rows starting from `row0`.
    long col0;
    long ncols;
    long row0;
    long nrows;

    // List of types (dt_Coltype) of each column returned. This list will have
    // `ncols` elements.
    PyObject *types;

    // Actual data within the window, represented as a PyList of PyLists of
    // Python primitives (such as PyLong, PyFloat, etc). The data is returned
    // in column-major order, i.e. each element of the list `data` represents
    // a single column from the parent datatable. The number of elements in
    // this list is `ncols`; and each element is a list of `nrows` items.
    PyObject *data;

} dt_DataWindowObject;


/** Python type corresponding to dt_DataWindowObject. */
PyTypeObject dt_DataWindowType;


#endif
