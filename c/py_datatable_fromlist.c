/**
 *
 *  Make Datatable from a Python list
 *
 */
#include "py_datatable.h"
#include "py_types.h"
#include "py_utils.h"

static Column* column_from_list(PyObject *list);


/**
 * Construct a new DataTable object from a python list.
 *
 * If the list is empty, then an empty (0 x 0) datatable is produced.
 * If the list is a list of lists, then inner lists are assumed to be the
 * columns, then the number of elements in these lists must be the same
 * and it will be the number of rows in the datatable produced.
 * Otherwise, we assume that the list represents a single data column, and
 * build the datatable appropriately.
 */
DataTable_PyObject*
pyDataTable_from_list_of_lists(PyTypeObject *type, PyObject *args)
{
    DataTable *dt = NULL;
    PyObject *list;

    if (!PyArg_ParseTuple(args, "O!:from_list", &PyList_Type, &list))
        return NULL;

    // Create a new (empty) DataTable instance
    dt = MALLOC(sizeof(DataTable));
    dt->source = NULL;
    dt->rowmapping = NULL;
    dt->columns = NULL;
    dt->nrows = 0;
    dt->ncols = 0;

    // If the supplied list is empty, return the empty Datatable object
    int64_t listsize = Py_SIZE(list);  // works both for lists and tuples
    if (listsize == 0) {
        return pyDataTable_from_DataTable(dt);
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
    dt->columns = CALLOC(sizeof(Column), dt->ncols);

    // Fill the data
    for (int64_t i = 0; i < dt->ncols; i++) {
        PyObject *src = PyList_GET_ITEM(list, i);
        Column *col = column_from_list(src);
        if (col == NULL) goto fail;
        memcpy(dt->columns + i, col, sizeof(Column));
        free(col);
    }

    return pyDataTable_from_DataTable(dt);

  fail:
    dt_DataTable_dealloc(dt, &dt_DataTable_dealloc_objcol);
    return NULL;
}



//---- Helper macros -----------------------------------------------------------
#define MIN(a, b)   ((a) < (b)? (a) : (b))
#define TYPE_SWITCH(s)  { stype = s; goto start_over; }
#define SET_I1B(v)  ((int8_t*)data)[i] = (v)
#define SET_I1I(v)  ((int8_t*)data)[i] = (v)
#define SET_I2I(v)  ((int16_t*)data)[i] = (v)
#define SET_I4I(v)  ((int32_t*)data)[i] = (v)
#define SET_I8I(v)  ((int64_t*)data)[i] = (v)
#define SET_F8R(v)  ((double*)data)[i] = (v)
#define SET_P8P(v)  ((PyObject**)data)[i] = (v)
#define DEFAULT(s)  default:                                                   \
                    PyErr_Format(PyExc_RuntimeError,                           \
                        "Stype %d has not been implemented for case %s",       \
                        stype, s);                                             \
                    goto fail;

// TODO: handle int64 buffers
#define SET_I4S(buf, len) {                                                    \
    size_t nextptr = strbuffer_ptr + (size_t) (len);                           \
    if (nextptr > strbuffer_size) {                                            \
        strbuffer_size = nextptr + (nrows - i - 1) * (nextptr / (i + 1));      \
        strbuffer = REALLOC(strbuffer, strbuffer_size);                        \
    }                                                                          \
    if ((len) > 0) {                                                           \
        memcpy(strbuffer + strbuffer_ptr, buf, (size_t) (len));                \
        strbuffer_ptr += (size_t) (len);                                       \
    }                                                                          \
    ((int32_t*)data)[i] = (int32_t) (strbuffer_ptr + 1);                       \
}

#define WRITE_STR(s) {                                                         \
    PyObject *pybytes = TRY(PyUnicode_AsEncodedString(s, "utf-8", "strict"));  \
    SET_I4S(PyBytes_AsString(pybytes), (size_t) PyBytes_GET_SIZE(pybytes));    \
    Py_DECREF(pybytes);                                                        \
}


/**
 * Create a single column of data from the python list.
 *
 * @param list the data source.
 * @returns pointer to a new Column object on success, or NULL on error
 */
Column* column_from_list(PyObject *list)
{
    if (list == NULL || !PyList_Check(list)) return NULL;

    Column *column = NULL;
    column = MALLOC(sizeof(Column));
    column->srcindex = -1;
    column->meta = NULL;
    column->data = NULL;
    column->mmapped = 0;

    size_t nrows = (size_t) Py_SIZE(list);
    if (nrows == 0) {
        column->stype = DT_REAL_F64;
        return 0;
    }

    DataSType stype = DT_VOID;
    void *data = NULL;
    char *strbuffer = NULL;
    size_t strbuffer_size = 0;
    size_t strbuffer_ptr = 0;
    int overflow = 0;
    while (stype < DT_STYPES_COUNT) {
        start_over:
        data = REALLOC(data, stype_info[stype].elemsize * nrows);
        if (stype == DT_STRING_I32_VCHAR) {
            strbuffer_size = MIN(nrows * 1000, 1 << 20);
            strbuffer_ptr = 0;
            strbuffer = MALLOC(strbuffer_size);
        } else if (strbuffer) {
            free(strbuffer);
            strbuffer = NULL;
            strbuffer_size = 0;
        }

        for (size_t i = 0; i < nrows; i++) {
            PyObject *item = PyList_GET_ITEM(list, i);  // borrowed ref
            PyTypeObject *itemtype = Py_TYPE(item);     // borrowed ref

            //---- store None value ----
            if (item == Py_None) {
                switch (stype) {
                    case DT_VOID:         /* do nothing */ break;
                    case DT_BOOLEAN_I8:   SET_I1B(NA_I8);  break;
                    case DT_INTEGER_I8:   SET_I1I(NA_I8);  break;
                    case DT_INTEGER_I16:  SET_I2I(NA_I16); break;
                    case DT_INTEGER_I32:  SET_I4I(NA_I32); break;
                    case DT_INTEGER_I64:  SET_I8I(NA_I64); break;
                    case DT_REAL_F64:     SET_F8R(NA_F64); break;
                    case DT_STRING_I32_VCHAR:
                        ((int32_t*)data)[i] = (int32_t) (-strbuffer_ptr-1);
                        break;
                    case DT_OBJECT_PYPTR: SET_P8P(none()); break;
                    DEFAULT("Py_None")
                }
            } else
            //---- store a boolean ----
            if (item == Py_True || item == Py_False) {
                int8_t val = (item == Py_True);
                switch (stype) {
                    case DT_BOOLEAN_I8:   SET_I1B(val);  break;
                    case DT_INTEGER_I8:   SET_I1I(val);  break;
                    case DT_INTEGER_I16:  SET_I2I((int16_t)val);  break;
                    case DT_INTEGER_I32:  SET_I4I((int32_t)val);  break;
                    case DT_INTEGER_I64:  SET_I8I((int64_t)val);  break;
                    case DT_REAL_F64:     SET_F8R((double)val);  break;
                    case DT_STRING_I32_VCHAR:
                        SET_I4S(val? "True" : "False", 5 - val);
                        break;
                    case DT_OBJECT_PYPTR: SET_P8P(incref(item));  break;
                    case DT_VOID:         TYPE_SWITCH(DT_BOOLEAN_I8);
                    DEFAULT("Py_True/Py_False")
                }
            } else
            //---- store an integer ----
            if (itemtype == &PyLong_Type || PyLong_Check(item)) {
                switch (stype) {
                    case DT_VOID:
                    case DT_BOOLEAN_I8:
                    case DT_INTEGER_I8:
                    case DT_INTEGER_I16:
                    case DT_INTEGER_I32:
                    case DT_INTEGER_I64: {
                        int64_t v = PyLong_AsInt64AndOverflow(item, &overflow);
                        if (overflow || v == NA_I64) TYPE_SWITCH(DT_REAL_F64);
                        int64_t aval = llabs(v);
                        if (stype == DT_BOOLEAN_I8 && (v == 0 || v == 1))
                            SET_I1B((int8_t)v);
                        else if (stype == DT_INTEGER_I8 && aval <= 127)
                            SET_I1I((int8_t)v);
                        else if (stype == DT_INTEGER_I16 && aval <= 32767)
                            SET_I2I((int16_t)v);
                        else if (stype == DT_INTEGER_I32 && aval <= INT32_MAX)
                            SET_I4I((int32_t)v);
                        else if (stype == DT_INTEGER_I64 && aval <= INT64_MAX)
                            SET_I8I(v);
                        else {
                            // stype is DT_VOID, or current stype is too small
                            // to hold the value `v`.
                            TYPE_SWITCH(aval <= 1? DT_BOOLEAN_I8 :
                                        aval <= 127? DT_INTEGER_I8 :
                                        aval <= 32767? DT_INTEGER_I16 :
                                        aval <= INT32_MAX? DT_INTEGER_I32 :
                                        aval <= INT64_MAX? DT_INTEGER_I64 :
                                        DT_REAL_F64);
                        }
                    } break;

                    case DT_REAL_F64: {
                        // TODO: check for overflows
                        SET_F8R(PyLong_AsDouble(item));
                    } break;

                    case DT_STRING_I32_VCHAR: {
                        PyObject *str = TRY(PyObject_Str(item));
                        WRITE_STR(str);
                        Py_DECREF(str);
                    } break;

                    case DT_OBJECT_PYPTR: {
                        SET_P8P(incref(item));
                    } break;

                    DEFAULT("PyLong_Type")
                }
            } else
            //---- store a real number ----
            if (itemtype == &PyFloat_Type || PyFloat_Check(item)) {
                // PyFloat_Type is just a wrapper around a plain double value.
                // The following call retrieves the underlying primitive:
                double val = PyFloat_AS_DOUBLE(item);
                switch (stype) {
                    case DT_REAL_F64:      SET_F8R(val);  break;
                    case DT_OBJECT_PYPTR:  SET_P8P(incref(item));  break;

                    case DT_VOID:
                    case DT_BOOLEAN_I8:
                    case DT_INTEGER_I8:
                    case DT_INTEGER_I16:
                    case DT_INTEGER_I32:
                    case DT_INTEGER_I64: {
                        // Split the double into integer and fractional parts
                        double n, f = modf(val, &n);
                        if (f == 0 && n <= INT64_MAX && n >= -INT64_MAX) {
                            int64_t v = (int64_t) n;
                            int64_t a = llabs(v);
                            if (stype == DT_BOOLEAN_I8 && (n == 0 || n == 1))
                                SET_I1B((int8_t)v);
                            else if (stype == DT_INTEGER_I8 && a <= 127)
                                SET_I1I((int8_t)v);
                            else if (stype == DT_INTEGER_I16 && a <= 32767)
                                SET_I2I((int16_t)v);
                            else if (stype == DT_INTEGER_I32 && a <= INT32_MAX)
                                SET_I4I((int32_t)v);
                            else if (stype == DT_INTEGER_I64)
                                SET_I8I(v);
                            else {
                                TYPE_SWITCH(n == 0 || n == 1? DT_BOOLEAN_I8 :
                                            a <= 127? DT_INTEGER_I8 :
                                            a <= 32767? DT_INTEGER_I16 :
                                            a <= INT32_MAX? DT_INTEGER_I32 :
                                            DT_INTEGER_I64);
                            }
                        } else {
                            TYPE_SWITCH(DT_REAL_F64);
                        }
                    } break;

                    case DT_STRING_I32_VCHAR: {
                        PyObject *str = TRY(PyObject_Str(item));
                        WRITE_STR(str);
                        Py_DECREF(str);
                    } break;

                    DEFAULT("PyFloat_Type")
                }
            } else
            //---- store a string ----
            if (itemtype == &PyUnicode_Type) {
                switch (stype) {
                    case DT_OBJECT_PYPTR:     SET_P8P(incref(item));  break;
                    case DT_STRING_I32_VCHAR: WRITE_STR(item);  break;
                    default:                  TYPE_SWITCH(DT_STRING_I32_VCHAR);
                }
            } else
            //---- store an object ----
            {
                if (stype == DT_OBJECT_PYPTR)
                    SET_P8P(incref(item));
                else
                    TYPE_SWITCH(DT_OBJECT_PYPTR);
            }
        } // end of `for (i = 0; i < nrows; i++)`

        // At this point `stype` can be DT_VOID if all values were `None`s. In
        // this case we just force the column to be DT_BOOLEAN_I8
        if (stype == DT_VOID)
            TYPE_SWITCH(DT_BOOLEAN_I8);

        if (stype == DT_STRING_I32_VCHAR) {
            size_t offoff = (strbuffer_ptr + (1 << 3) - 1) >> 3 << 3;
            size_t final_size = offoff + 4 * (size_t)nrows;
            strbuffer = REALLOC(strbuffer, final_size);
            memcpy(strbuffer + offoff, data, 4 * (size_t)nrows);
            data = strbuffer;
            column->meta = MALLOC(sizeof(VarcharMeta));
            ((VarcharMeta*)(column->meta))->offoff = (int64_t) offoff;
        }

        column->data = data;
        column->stype = stype;
        return column;
    }

  fail:
    if (column) {
        free(column->data);
        free(column->meta);
        free(column);
    }
    return NULL;
}
