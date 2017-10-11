/**
 *
 *  Make Datatable from a Python list
 *
 */
#include "column.h"
#include "memorybuf.h"
#include "py_datatable.h"
#include "py_types.h"
#include "py_utils.h"

static const double INFD = (double)INFINITY;


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
    Column **cols = NULL;
    PyObject *list;

    if (!PyArg_ParseTuple(args, "O!:from_list", &PyList_Type, &list))
        return NULL;


    // If the supplied list is empty, return the empty Datatable object
    int64_t listsize = Py_SIZE(list);  // works both for lists and tuples
    if (listsize == 0) {
        dtmalloc(cols, Column*, 1);
        cols[0] = NULL;
        return pydt_from_dt(new DataTable(cols));
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

    dtcalloc(cols, Column*, listsize + 1);

    // Fill the data
    for (int64_t i = 0; i < listsize; i++) {
        PyObject *src = PyList_GET_ITEM(list, i);
        cols[i] = (Column*) column_from_list(src);
        if (!cols[i]) goto fail;
    }

    return pydt_from_dt(new DataTable(cols));

  fail:
    if (cols) {
        for (int i = 0; cols[i] != NULL; ++i) {
            delete cols[i];
        }
        dtfree(cols);
    }
    return NULL;
}



//---- Helper macros -----------------------------------------------------------
#define MIN(a, b)   ((a) < (b)? (a) : (b))
#define TYPE_SWITCH(s)  { stype = s; goto start_over; }
#define DEFAULT(s)  default:                                                   \
                    PyErr_Format(PyExc_RuntimeError,                           \
                        "Stype %d has not been implemented for case %s",       \
                        stype, s);                                             \
                    goto fail;

// TODO: handle int64 buffers
#define SET_I4S(buf, len) {                                                    \
    size_t nextptr = strbuffer_ptr + (size_t) (len);                           \
    if (nextptr > strbuffer_size) {                                            \
        strbuffer_size = nextptr + (size_t)(nrows - i - 1) * (nextptr / (size_t)(i + 1));      \
        dtrealloc(strbuffer, char, strbuffer_size);                            \
    }                                                                          \
    if ((len) > 0) {                                                           \
        memcpy(strbuffer + strbuffer_ptr, buf, (size_t) (len));                \
        strbuffer_ptr += (size_t) (len);                                       \
    }                                                                          \
    mb->set_elem<int32_t>(i, (int32_t) (strbuffer_ptr + 1));                   \
}

#define WRITE_STR(s) {                                                         \
    PyObject *pybytes = PyUnicode_AsEncodedString(s, "utf-8", "strict");       \
    if (!pybytes) goto fail;                                                   \
    SET_I4S(PyBytes_AsString(pybytes), (size_t) PyBytes_GET_SIZE(pybytes));    \
    Py_DECREF(pybytes);                                                        \
}


/**
 * Create a single column of data from the python list.
 *
 * @param list the data source.
 * @returns pointer to a new Column object on success, or NULL on error
 */
// TODO: make this a constructor
Column* column_from_list(PyObject *list)
{
    if (list == NULL || !PyList_Check(list)) return NULL;

    int64_t nrows = Py_SIZE(list);
    if (nrows == 0) {
        return Column::new_data_column(ST_BOOLEAN_I1, 0);
    }

    SType stype = ST_VOID;
    MemoryMemBuf* mb = new MemoryMemBuf(0);
    char *strbuffer = NULL;
    size_t strbuffer_size = 0;
    size_t strbuffer_ptr = 0;
    int overflow = 0;
    while (stype < DT_STYPES_COUNT) {
        start_over: {}
        mb->resize(stype_info[stype].elemsize * (size_t)nrows);
        if (stype == ST_STRING_I4_VCHAR) {
            strbuffer_size = MIN((size_t)nrows * 1000, 1 << 20);
            strbuffer_ptr = 0;
            dtmalloc(strbuffer, char, strbuffer_size);
        } else if (strbuffer) {
            dtfree(strbuffer);
            strbuffer_size = 0;
        }

        for (int64_t i = 0; i < nrows; i++) {
            PyObject *item = PyList_GET_ITEM(list, i);  // borrowed ref
            PyTypeObject *itemtype = Py_TYPE(item);     // borrowed ref

            //---- store None value ----
            if (item == Py_None) {
                switch (stype) {
                    case ST_VOID:        /* do nothing */ break;
                    case ST_BOOLEAN_I1:
                    case ST_INTEGER_I1:  mb->set_elem<int8_t>(i, NA_I1);  break;
                    case ST_INTEGER_I2:  mb->set_elem<int16_t>(i, NA_I2); break;
                    case ST_INTEGER_I4:  mb->set_elem<int32_t>(i, NA_I4); break;
                    case ST_INTEGER_I8:  mb->set_elem<int64_t>(i, NA_I8); break;
                    case ST_REAL_F8:     mb->set_elem<double>(i, NA_F8); break;
                    case ST_STRING_I4_VCHAR:
                        mb->set_elem<int32_t>(i, (int32_t) (-strbuffer_ptr-1));
                        break;
                    case ST_OBJECT_PYPTR: mb->set_elem<PyObject*>(i, none()); break;
                    DEFAULT("Py_None")
                }
            } else
            //---- store a boolean ----
            if (item == Py_True || item == Py_False) {
                int8_t val = (item == Py_True);
                switch (stype) {
                    case ST_BOOLEAN_I1:
                    case ST_INTEGER_I1:  mb->set_elem<int8_t>(i, val);  break;
                    case ST_INTEGER_I2:  mb->set_elem<int16_t>(i, (int16_t)val);  break;
                    case ST_INTEGER_I4:  mb->set_elem<int32_t>(i, (int32_t)val);  break;
                    case ST_INTEGER_I8:  mb->set_elem<int64_t>(i, (int64_t)val);  break;
                    case ST_REAL_F8:     mb->set_elem<double>(i, (double)val);  break;
                    case ST_STRING_I4_VCHAR:
                        SET_I4S(val? "True" : "False", 5 - val);
                        break;
                    case ST_OBJECT_PYPTR: mb->set_elem<PyObject*>(i, incref(item));  break;
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
                            mb->set_elem<int8_t>(i, (int8_t)v);
                        else if (stype == ST_INTEGER_I1 && aval <= 127)
                            mb->set_elem<int8_t>(i, (int8_t)v);
                        else if (stype == ST_INTEGER_I2 && aval <= 32767)
                            mb->set_elem<int16_t>(i, (int16_t)v);
                        else if (stype == ST_INTEGER_I4 && aval <= INT32_MAX)
                            mb->set_elem<int32_t>(i, (int32_t)v);
                        else if (stype == ST_INTEGER_I8 && aval <= INT64_MAX)
                            mb->set_elem<int64_t>(i, v);
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
                        double res = PyLong_AsDouble(item);
                        if (res == -1 && PyErr_Occurred()) {
                            if (!PyErr_ExceptionMatches(PyExc_OverflowError))
                                goto fail;
                            PyErr_Clear();
                            int sign = _PyLong_Sign(item);
                            mb->set_elem<double>(i, sign > 0? INFD : -INFD);
                        } else {
                            mb->set_elem<double>(i, res);
                        }
                    } break;

                    case ST_STRING_I4_VCHAR: {
                        PyObject *str = PyObject_Str(item);
                        if (!str) goto fail;
                        WRITE_STR(str);
                        Py_DECREF(str);
                    } break;

                    case ST_OBJECT_PYPTR: {
                        mb->set_elem<PyObject*>(i, incref(item));
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
                    case ST_REAL_F8:       mb->set_elem<double>(i, val);  break;
                    case ST_OBJECT_PYPTR:  mb->set_elem<PyObject*>(i, incref(item));  break;

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
                                mb->set_elem<int8_t>(i, (int8_t)v);
                            else if (stype == ST_INTEGER_I1 && a <= 127)
                                mb->set_elem<int8_t>(i, (int8_t)v);
                            else if (stype == ST_INTEGER_I2 && a <= 32767)
                                mb->set_elem<int16_t>(i, (int16_t)v);
                            else if (stype == ST_INTEGER_I4 && a <= INT32_MAX)
                                mb->set_elem<int32_t>(i, (int32_t)v);
                            else if (stype == ST_INTEGER_I8)
                                mb->set_elem<int64_t>(i, v);
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
                        PyObject *str = PyObject_Str(item);
                        if (!str) goto fail;
                        WRITE_STR(str);
                        Py_DECREF(str);
                    } break;

                    DEFAULT("PyFloat_Type")
                }
            } else
            //---- store a string ----
            if (itemtype == &PyUnicode_Type) {
                switch (stype) {
                    case ST_OBJECT_PYPTR:     mb->set_elem<PyObject*>(i, incref(item));  break;
                    case ST_STRING_I4_VCHAR:  WRITE_STR(item);  break;
                    default:                  TYPE_SWITCH(ST_STRING_I4_VCHAR);
                }
            } else
            //---- store an object ----
            {
                if (stype == ST_OBJECT_PYPTR)
                    mb->set_elem<PyObject*>(i, incref(item));
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
            size_t padding_size = Column::i4s_padding(strbuffer_ptr);
            size_t offoff = strbuffer_ptr + padding_size;
            size_t final_size = offoff + esz * (size_t)nrows;
            dtrealloc(strbuffer, char, final_size);
            memset(strbuffer + strbuffer_ptr, 0xFF, padding_size);
            memcpy(strbuffer + offoff, mb->get(), esz * (size_t)nrows);
            Column *column = new Column((size_t)nrows, stype);
            column->mbuf = new MemoryMemBuf(strbuffer, final_size);
            ((VarcharMeta*) column->meta)->offoff = (int64_t) offoff;
            return column;
        } else {
            Column *column = new Column((size_t)nrows, stype);
            column->mbuf = mb;
            return column;
        }
    }

  fail:
    return NULL;
}
