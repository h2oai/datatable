/**
 *
 *  Make Datatable from a Python list
 *
 */
#include "py_datatable.h"
#include "dtutils.h"

static int _fill_1_column(PyObject *list, Column *column);
static int _switch_to_coltype(ColType newtype, PyObject *list, Column *column);


/**
 * Construct a new DataTable object from a python list.
 *
 * If the list is empty, then an empty (0 x 0) datatable is produced.
 * If the list is a list of lists, then inner lists are assumed to be the
 * columns, then the number of elements in these lists must be the same
 * and it will be the number of rows in the datatable produced.
 * Otherwise, we assume that the list represents a single data column, and
 * build the datatable appropriately.
 *
 * :param list:
 *     A python list of python lists representing the data to be ingested.
 */
DataTable_PyObject* dt_DataTable_fromlist(PyTypeObject *type, PyObject *args)
{
    DataTable_PyObject *self = NULL;
    DataTable *dt = NULL;
    PyObject *list;

    if (!PyArg_ParseTuple(args, "O!:from_list", &PyList_Type, &list))
        return NULL;

    // Create a new (empty) DataTable instance
    self = DataTable_PyNew();
    dt = malloc(sizeof(DataTable));
    if (self == NULL || dt == NULL) goto fail;
    dt->source = NULL;
    dt->rowindex = NULL;
    dt->columns = NULL;

    // if the supplied list is empty, return the empty Datatable object
    int64_t listsize = Py_SIZE(list);  // works both for lists and tuples
    if (listsize == 0) {
        dt->nrows = 0;
        dt->ncols = 0;
        self->ref = dt;
        return self;
    }

    // Basic check validity of the provided data.
    int64_t item0size = Py_SIZE(PyList_GET_ITEM(list, 0));
    for (int64_t i = 0; i < listsize; ++i) {
        PyObject *item = PyList_GET_ITEM(list, i);
        if (!PyList_Check(item)) {
            PyErr_SetString(PyExc_ValueError,
                            "Source list is not list-of-lists");
            goto fail;
        }
        if (Py_SIZE(item) != item0size) {
            PyErr_SetString(PyExc_ValueError,
                            "Source lists have variable number of rows");
            goto fail;
        }
    }

    dt->ncols = listsize;
    dt->nrows = item0size;

    // Allocate memory for the datatable's columns
    dt->columns = calloc(sizeof(Column), dt->ncols);
    if (dt->columns == NULL) goto fail;

    // Fill the data
    Column *col = dt->columns;
    for (int64_t i = 0; i < dt->ncols; ++i, col++) {
        col->type = DT_AUTO;
        col->srcindex = -1;
        PyObject *src = PyList_GET_ITEM(list, i);
        int ret = _fill_1_column(src, col);
        if (ret == -1) goto fail;
    }

    self->ref = dt;
    return self;

  fail:
    Py_XDECREF(self);
    dt_DataTable_dealloc(dt, &dt_DataTable_dealloc_objcol);
    return NULL;
}


/**
 * Create a single column of data from the python list.
 *
 * @param list: the data source.
 * @param coltype: the desired dtype for the column; if DT_AUTO then this value
 *     will be modified in-place to an appropriate type.
 * @param coldata: pointer to a ``ColType`` structure that will be modified
 *     by reference to fill in the column's data.
 * @returns 0 on success, -1 on error
 */
static int _fill_1_column(PyObject *list, Column *column) {
    long nrows = (long) Py_SIZE(list);
    if (nrows == 0) {
        column->data = NULL;
        column->type = DT_DOUBLE;
        return 0;
    }

    column->data = malloc(ColType_size[column->type] * nrows);
    if (column->data == NULL) return -1;

    int overflow;
    void *col = column->data;
    for (long i = 0; i < nrows; ++i) {
        PyObject *item = PyList_GET_ITEM(list, i);  // borrowed ref
        PyTypeObject *itemtype = Py_TYPE(item);     // borrowed ref

        if (item == Py_None) {
            //---- store NaN value ----
            switch (column->type) {
                case DT_DOUBLE: ((double*)col)[i] = NAN; break;
                case DT_LONG:   ((long*)col)[i] = LONG_MIN; break;
                case DT_BOOL:   ((char*)col)[i] = 2; break;
                case DT_STRING: ((char**)col)[i] = NULL; break;
                case DT_OBJECT: ((PyObject**)col)[i] = incref(item); break;
                case DT_AUTO:   /* do nothing */ break;
            }

        } else if (itemtype == &PyLong_Type) {
            //---- store an integer ----
            long_case:
            switch (column->type) {
                case DT_LONG: {
                    long val = PyLong_AsLongAndOverflow(item, &overflow);
                    if (overflow)
                        return _switch_to_coltype(DT_DOUBLE, list, column);
                    ((long*)col)[i] = val;
                }   break;

                case DT_DOUBLE:
                    ((double*)col)[i] = PyLong_AsDouble(item);
                    break;

                case DT_BOOL: {
                    long val = PyLong_AsLongAndOverflow(item, &overflow);
                    if (overflow || (val != 0 && val != 1))
                        return _switch_to_coltype(overflow? DT_DOUBLE : DT_LONG, list, column);
                    ((char*)col)[i] = (unsigned char) val;
                } break;

                case DT_STRING:
                    // not supported yet
                    return _switch_to_coltype(DT_OBJECT, list, column);

                case DT_OBJECT:
                    ((PyObject**)col)[i] = incref(item);
                    break;

                case DT_AUTO: {
                    long val = PyLong_AsLongAndOverflow(item, &overflow);
                    return _switch_to_coltype(
                        (val == 0 || val == 1) && !overflow? DT_BOOL :
                        overflow? DT_DOUBLE : DT_LONG, list, column
                    );
                }
            }

        } else if (itemtype == &PyFloat_Type) {
            //---- store a real number ----
            float_case: {}
            double val = PyFloat_AS_DOUBLE(item);
            switch (column->type) {
                case DT_DOUBLE:
                    ((double*)col)[i] = val;
                    break;

                case DT_LONG: {
                    double intpart, fracpart = modf(val, &intpart);
                    if (fracpart != 0 || intpart <= LONG_MIN || intpart >LONG_MAX)
                        return _switch_to_coltype(DT_DOUBLE, list, column);
                    ((long*)col)[i] = (long) intpart;
                }   break;

                case DT_BOOL: {
                    if (val != 0 && val != 1)
                        return _switch_to_coltype(DT_DOUBLE, list, column);
                    ((char*)col)[i] = (char) (val == 1);
                }   break;

                case DT_STRING:
                    // not supported yet
                    return _switch_to_coltype(DT_OBJECT, list, column);

                case DT_OBJECT:
                    ((PyObject**)col)[i] = incref(item);
                    break;

                case DT_AUTO: {
                    double intpart, fracpart = modf(val, &intpart);
                    return _switch_to_coltype(
                        val == 0 || val == 1? DT_BOOL :
                        fracpart == 0 && (LONG_MIN < intpart && intpart < LONG_MAX)? DT_LONG : DT_DOUBLE,
                        list, column
                    );
                }
            }

        } else if (itemtype == &PyBool_Type) {
            unsigned char val = (item == Py_True);
            switch (column->type) {
                case DT_BOOL:   ((char*)col)[i] = val;  break;
                case DT_LONG:   ((long*)col)[i] = (long) val;  break;
                case DT_DOUBLE: ((double*)col)[i] = (double) val;  break;
                case DT_STRING: ((char**)col)[i] = val? strdup("1") : strdup("0"); break;
                case DT_OBJECT: ((PyObject**)col)[i] = incref(item);  break;
                case DT_AUTO:   return _switch_to_coltype(DT_BOOL, list, column);
            }

        } else if (itemtype == &PyUnicode_Type) {
            return _switch_to_coltype(DT_OBJECT, list, column);

        } else {
            if (column->type == DT_OBJECT) {
                ((PyObject**)col)[i] = incref(item);
            } else {
                // These checks will be true only if someone subclassed base
                // types, in which case we still want to treat them as
                // primitives but avoid more expensive Py*_Check in the
                // common case.
                if (PyLong_Check(item)) goto long_case;
                if (PyFloat_Check(item)) goto float_case;

                return _switch_to_coltype(DT_OBJECT, list, column);
            }
        }
    }

    // If all values in the column were NaNs, then cast that column as DT_DOUBLE
    if (column->type == DT_AUTO) {
        return _switch_to_coltype(DT_DOUBLE, list, column);
    }
    return 0;
}



/**
 * Switch to a different column type and then re-run `_fill_1_column()`.
 */
static int _switch_to_coltype(ColType newtype, PyObject *list, Column *column) {
    free(column->data);
    column->type = newtype;
    return _fill_1_column(list, column);
}
