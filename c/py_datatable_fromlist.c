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
PyObject* pydatatable_from_list(UU, PyObject *args)
{
    DataTable *dt = NULL;
    PyObject *list;

    if (!PyArg_ParseTuple(args, "O!:from_list", &PyList_Type, &list))
        return NULL;

    // Create a new (empty) DataTable instance
    dtmalloc(dt, DataTable, 1);
    dt->rowindex = NULL;
    dt->columns = NULL;
    dt->nrows = 0;
    dt->ncols = 0;

    // If the supplied list is empty, return the empty Datatable object
    int64_t listsize = Py_SIZE(list);  // works both for lists and tuples
    if (listsize == 0) {
        dtmalloc(dt->columns, Column*, 1);
        dt->columns[0] = NULL;
        return pydt_from_dt(dt);
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
    dtmalloc(dt->columns, Column*, dt->ncols + 1);
    dt->columns[dt->ncols] = NULL;

    // Fill the data
    for (int64_t i = 0; i < dt->ncols; i++) {
        PyObject *src = PyList_GET_ITEM(list, i);
        dt->columns[i] = TRY(column_from_list(src));
    }

    return pydt_from_dt(dt);

  fail:
    datatable_dealloc(dt);
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
        dtrealloc(strbuffer, char, strbuffer_size);                            \
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

    Column *column = make_data_column(ST_BOOLEAN_I1, 0);

    size_t nrows = (size_t) Py_SIZE(list);
    if (nrows == 0) {
        return column;
    }

    SType stype = ST_VOID;
    void *data = NULL;
    char *strbuffer = NULL;
    size_t strbuffer_size = 0;
    size_t strbuffer_ptr = 0;
    int overflow = 0;
    while (stype < DT_STYPES_COUNT) {
        start_over: {}
        size_t alloc_size = stype_info[stype].elemsize * (size_t)nrows;
        dtrealloc(data, void, alloc_size);
        column->nrows = (int64_t) nrows;
        column->alloc_size = alloc_size;
        if (stype == ST_STRING_I4_VCHAR) {
            strbuffer_size = MIN(nrows * 1000, 1 << 20);
            strbuffer_ptr = 0;
            dtmalloc(strbuffer, char, strbuffer_size);
        } else if (strbuffer) {
            dtfree(strbuffer);
            strbuffer_size = 0;
        }

        for (size_t i = 0; i < nrows; i++) {
            PyObject *item = PyList_GET_ITEM(list, i);  // borrowed ref
            PyTypeObject *itemtype = Py_TYPE(item);     // borrowed ref

            //---- store None value ----
            if (item == Py_None) {
                switch (stype) {
                    case ST_VOID:        /* do nothing */ break;
                    case ST_BOOLEAN_I1:  SET_I1B(NA_I1);  break;
                    case ST_INTEGER_I1:  SET_I1I(NA_I1);  break;
                    case ST_INTEGER_I2:  SET_I2I(NA_I2); break;
                    case ST_INTEGER_I4:  SET_I4I(NA_I4); break;
                    case ST_INTEGER_I8:  SET_I8I(NA_I8); break;
                    case ST_REAL_F8:     SET_F8R(NA_F8); break;
                    case ST_STRING_I4_VCHAR:
                        ((int32_t*)data)[i] = (int32_t) (-strbuffer_ptr-1);
                        break;
                    case ST_OBJECT_PYPTR: SET_P8P(none()); break;
                    DEFAULT("Py_None")
                }
            } else
            //---- store a boolean ----
            if (item == Py_True || item == Py_False) {
                int8_t val = (item == Py_True);
                switch (stype) {
                    case ST_BOOLEAN_I1:  SET_I1B(val);  break;
                    case ST_INTEGER_I1:  SET_I1I(val);  break;
                    case ST_INTEGER_I2:  SET_I2I((int16_t)val);  break;
                    case ST_INTEGER_I4:  SET_I4I((int32_t)val);  break;
                    case ST_INTEGER_I8:  SET_I8I((int64_t)val);  break;
                    case ST_REAL_F8:     SET_F8R((double)val);  break;
                    case ST_STRING_I4_VCHAR:
                        SET_I4S(val? "True" : "False", 5 - val);
                        break;
                    case ST_OBJECT_PYPTR: SET_P8P(incref(item));  break;
                    case ST_VOID:         TYPE_SWITCH(ST_BOOLEAN_I1);
                    DEFAULT("Py_True/Py_False")
                }
            } else
            //---- store an integer ----
            if (itemtype == &PyLong_Type || PyLong_Check(item)) {
                switch (stype) {
                    case ST_VOID:
                    case ST_BOOLEAN_I1:
                    case ST_INTEGER_I1:
                    case ST_INTEGER_I2:
                    case ST_INTEGER_I4:
                    case ST_INTEGER_I8: {
                        int64_t v = PyLong_AsInt64AndOverflow(item, &overflow);
                        if (overflow || v == NA_I8) TYPE_SWITCH(ST_REAL_F8);
                        int64_t aval = llabs(v);
                        if (stype == ST_BOOLEAN_I1 && (v == 0 || v == 1))
                            SET_I1B((int8_t)v);
                        else if (stype == ST_INTEGER_I1 && aval <= 127)
                            SET_I1I((int8_t)v);
                        else if (stype == ST_INTEGER_I2 && aval <= 32767)
                            SET_I2I((int16_t)v);
                        else if (stype == ST_INTEGER_I4 && aval <= INT32_MAX)
                            SET_I4I((int32_t)v);
                        else if (stype == ST_INTEGER_I8 && aval <= INT64_MAX)
                            SET_I8I(v);
                        else {
                            // stype is ST_VOID, or current stype is too small
                            // to hold the value `v`.
                            TYPE_SWITCH(v == 0 || v == 1? ST_BOOLEAN_I1 :
                                        aval <= 127? ST_INTEGER_I1 :
                                        aval <= 32767? ST_INTEGER_I2 :
                                        aval <= INT32_MAX? ST_INTEGER_I4 :
                                        aval <= INT64_MAX? ST_INTEGER_I8 :
                                        ST_REAL_F8);
                        }
                    } break;

                    case ST_REAL_F8: {
                        // TODO: check for overflows
                        SET_F8R(PyLong_AsDouble(item));
                    } break;

                    case ST_STRING_I4_VCHAR: {
                        PyObject *str = TRY(PyObject_Str(item));
                        WRITE_STR(str);
                        Py_DECREF(str);
                    } break;

                    case ST_OBJECT_PYPTR: {
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
                    case ST_REAL_F8:       SET_F8R(val);  break;
                    case ST_OBJECT_PYPTR:  SET_P8P(incref(item));  break;

                    case ST_VOID:
                    case ST_BOOLEAN_I1:
                    case ST_INTEGER_I1:
                    case ST_INTEGER_I2:
                    case ST_INTEGER_I4:
                    case ST_INTEGER_I8: {
                        // Split the double into integer and fractional parts
                        double n, f = modf(val, &n);
                        if (f == 0 && n <= INT64_MAX && n >= -INT64_MAX) {
                            int64_t v = (int64_t) n;
                            int64_t a = llabs(v);
                            if (stype == ST_BOOLEAN_I1 && (n == 0 || n == 1))
                                SET_I1B((int8_t)v);
                            else if (stype == ST_INTEGER_I1 && a <= 127)
                                SET_I1I((int8_t)v);
                            else if (stype == ST_INTEGER_I2 && a <= 32767)
                                SET_I2I((int16_t)v);
                            else if (stype == ST_INTEGER_I4 && a <= INT32_MAX)
                                SET_I4I((int32_t)v);
                            else if (stype == ST_INTEGER_I8)
                                SET_I8I(v);
                            else {
                                TYPE_SWITCH(n == 0 || n == 1? ST_BOOLEAN_I1 :
                                            a <= 127? ST_INTEGER_I1 :
                                            a <= 32767? ST_INTEGER_I2 :
                                            a <= INT32_MAX? ST_INTEGER_I4 :
                                            ST_INTEGER_I8);
                            }
                        } else {
                            TYPE_SWITCH(ST_REAL_F8);
                        }
                    } break;

                    case ST_STRING_I4_VCHAR: {
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
                    case ST_OBJECT_PYPTR:     SET_P8P(incref(item));  break;
                    case ST_STRING_I4_VCHAR:  WRITE_STR(item);  break;
                    default:                  TYPE_SWITCH(ST_STRING_I4_VCHAR);
                }
            } else
            //---- store an object ----
            {
                if (stype == ST_OBJECT_PYPTR)
                    SET_P8P(incref(item));
                else
                    TYPE_SWITCH(ST_OBJECT_PYPTR);
            }
        } // end of `for (i = 0; i < nrows; i++)`

        // At this point `stype` can be ST_VOID if all values were `None`s. In
        // this case we just force the column to be ST_BOOLEAN_I1
        if (stype == ST_VOID)
            TYPE_SWITCH(ST_BOOLEAN_I1);

        if (stype == ST_STRING_I4_VCHAR) {
            size_t esz = sizeof(int32_t);
            size_t padding_size = column_i4s_padding(strbuffer_ptr);
            size_t offoff = strbuffer_ptr + padding_size;
            size_t final_size = offoff + esz * (size_t)nrows;
            dtrealloc(strbuffer, char, final_size);
            memset(strbuffer + strbuffer_ptr, 0xFF, padding_size);
            memcpy(strbuffer + offoff, data, esz * (size_t)nrows);
            data = strbuffer;
            column->alloc_size = final_size;
            dtmalloc(column->meta, VarcharMeta, 1);
            ((VarcharMeta*) column->meta)->offoff = (int64_t) offoff;
        }

        column->data = data;
        column->stype = stype;
        return column;
    }

  fail:
    column_decref(column);
    return NULL;
}
