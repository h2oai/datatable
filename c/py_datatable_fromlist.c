/**
 *
 *  Make Datatable from a Python list
 *
 */
#include "py_datatable.h"
#include "py_utils.h"

static int _fill_1_column(PyObject *list, Column *column);
static int _switch_to_coltype(DataSType newtype, PyObject *list, Column *column);


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
    dt->rowmapping = NULL;
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
    dt->columns = calloc(sizeof(Column), (size_t)dt->ncols);
    if (dt->columns == NULL) goto fail;

    // Fill the data
    Column *col = dt->columns;
    for (int64_t i = 0; i < dt->ncols; ++i, col++) {
        col->stype = DT_VOID;
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
 * @param list the data source.
 * @param column the column object to be filled with data.
 * @returns 0 on success, -1 on error
 */
static int _fill_1_column(PyObject *list, Column *column) {
    int64_t nrows = (int64_t) Py_SIZE(list);
    if (nrows == 0) {
        column->data = NULL;
        column->stype = DT_REAL_F64;
        return 0;
    }

    size_t elemsize = stype_info[column->stype].elemsize;
    column->data = malloc(elemsize * (size_t)nrows);
    if (column->data == NULL) return -1;

    int overflow;
    void *col = column->data;
    for (int64_t i = 0; i < nrows; ++i) {
        PyObject *item = PyList_GET_ITEM(list, i);  // borrowed ref
        PyTypeObject *itemtype = Py_TYPE(item);     // borrowed ref

        if (item == Py_None) {
            //---- store NaN value ----
            switch (column->stype) {
                case DT_VOID:         /* do nothing */ break;
                case DT_REAL_F64:     ((double*)col)[i] = NA_F64; break;
                case DT_INTEGER_I64:  ((int64_t*)col)[i] = NA_I64; break;
                case DT_BOOLEAN_I8:   ((int8_t*)col)[i] = NA_I8; break;
                case DT_OBJECT_PYPTR: ((PyObject**)col)[i] = none(); break;
                default: assert(0);   // not implemented
            }

        } else if (itemtype == &PyLong_Type) {
            //---- store an integer ----
            long_case:
            switch (column->stype) {
                case DT_INTEGER_I64: {
                    int64_t val = PyLong_AsLongAndOverflow(item, &overflow);
                    if (overflow)
                        return _switch_to_coltype(DT_REAL_F64, list, column);
                    ((int64_t*)col)[i] = val;
                }   break;

                case DT_REAL_F64:
                    ((double*)col)[i] = PyLong_AsDouble(item);
                    break;

                case DT_BOOLEAN_I8: {
                    int64_t val = PyLong_AsLongAndOverflow(item, &overflow);
                    if (overflow || (val != 0 && val != 1))
                        return _switch_to_coltype(overflow? DT_REAL_F64 : DT_INTEGER_I64, list, column);
                    ((unsigned char*)col)[i] = (unsigned char) val;
                } break;

                case DT_OBJECT_PYPTR:
                    ((PyObject**)col)[i] = incref(item);
                    break;

                case DT_VOID: {
                    int64_t val = PyLong_AsLongAndOverflow(item, &overflow);
                    return _switch_to_coltype(
                        (val == 0 || val == 1) && !overflow? DT_BOOLEAN_I8 :
                        overflow? DT_REAL_F64 : DT_INTEGER_I64, list, column
                    );
                }
                default: assert(0);
            }

        } else if (itemtype == &PyFloat_Type) {
            //---- store a real number ----
            float_case: {}
            double val = PyFloat_AS_DOUBLE(item);
            switch (column->stype) {
                case DT_REAL_F64:
                    ((double*)col)[i] = val;
                    break;

                case DT_INTEGER_I64: {
                    double intpart, fracpart = modf(val, &intpart);
                    if (fracpart != 0 || intpart <= LLONG_MIN || intpart > LLONG_MAX)
                        return _switch_to_coltype(DT_REAL_F64, list, column);
                    ((int64_t*)col)[i] = (int64_t) intpart;
                }   break;

                case DT_BOOLEAN_I8: {
                    if (val != 0 && val != 1)
                        return _switch_to_coltype(DT_REAL_F64, list, column);
                    ((char*)col)[i] = (char) (val == 1);
                }   break;

                case DT_OBJECT_PYPTR:
                    ((PyObject**)col)[i] = incref(item);
                    break;

                case DT_VOID: {
                    double intpart, fracpart = modf(val, &intpart);
                    return _switch_to_coltype(
                        val == 0 || val == 1? DT_BOOLEAN_I8 :
                        fracpart == 0 && (LONG_MIN < intpart && intpart < LONG_MAX)? DT_INTEGER_I64 : DT_REAL_F64,
                        list, column
                    );
                }
                default: assert(0);   // not implemented
            }

        } else if (itemtype == &PyBool_Type) {
            unsigned char val = (item == Py_True);
            switch (column->stype) {
                case DT_BOOLEAN_I8:   ((unsigned char*)col)[i] = val;  break;
                case DT_INTEGER_I64:  ((int64_t*)col)[i] = (int64_t) val;  break;
                case DT_REAL_F64:     ((double*)col)[i] = (double) val;  break;
                case DT_OBJECT_PYPTR: ((PyObject**)col)[i] = incref(item);  break;
                case DT_VOID:         return _switch_to_coltype(DT_BOOLEAN_I8, list, column);
                default: assert(0);   // not implemented
            }

        } else if (itemtype == &PyUnicode_Type) {
            return _switch_to_coltype(DT_OBJECT_PYPTR, list, column);

        } else {
            if (column->stype == DT_OBJECT_PYPTR) {
                ((PyObject**)col)[i] = incref(item);
            } else {
                // These checks will be true only if someone subclassed base
                // types, in which case we still want to treat them as
                // primitives but avoid more expensive Py*_Check in the
                // common case.
                if (PyLong_Check(item)) goto long_case;
                if (PyFloat_Check(item)) goto float_case;

                return _switch_to_coltype(DT_OBJECT_PYPTR, list, column);
            }
        }
    }

    // If all values in the column were NaNs, then cast that column as DT_REAL_F64
    if (column->stype == DT_VOID) {
        return _switch_to_coltype(DT_REAL_F64, list, column);
    }
    return 0;
}



/**
 * Switch to a different column type and then re-run `_fill_1_column()`.
 */
static int _switch_to_coltype(DataSType newtype, PyObject *list, Column *column) {
    free(column->data);
    column->stype = newtype;
    return _fill_1_column(list, column);
}
