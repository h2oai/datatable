#include "datatable.h"
#include <exception>
#include <iostream>
#include <vector>
#include "datatable_check.h"
#include "py_column.h"
#include "py_columnset.h"
#include "py_datatable.h"
#include "py_datawindow.h"
#include "py_rowindex.h"
#include "py_types.h"
#include "py_utils.h"

// Forward declarations
static PyObject *strRowIndexTypeArr32;
static PyObject *strRowIndexTypeArr64;
static PyObject *strRowIndexTypeSlice;


/**
 * Create a new DataTable_PyObject by wrapping the provided DataTable `dt`.
 * If `dt` is NULL then this function also returns NULL.
 */
PyObject* pydt_from_dt(DataTable *dt)
{
    if (dt == NULL) return NULL;
    PyObject *pydt = PyObject_CallObject((PyObject*) &DataTable_PyType, NULL);
    if (pydt) {
        auto pypydt = reinterpret_cast<DataTable_PyObject*>(pydt);
        pypydt->ref = dt;
        pypydt->use_stype_for_buffers = ST_VOID;
    }
    return pydt;
}
#define py pydt_from_dt


/**
 * Attempt to extract a DataTable from the given PyObject, iff it is a
 * DataTable_PyObject. On success, the reference to the DataTable is stored in
 * the `address` and 1 is returned. On failure, returns 0.
 *
 * This function does not attempt to DECREF the source PyObject.
 */
int dt_unwrap(PyObject *object, DataTable **address) {
    if (!object) return 0;
    if (!PyObject_TypeCheck(object, &DataTable_PyType)) {
        PyErr_SetString(PyExc_TypeError, "Expected object of type DataTable");
        return 0;
    }
    *address = ((DataTable_PyObject*)object)->ref;
    return 1;
}



//------------------------------------------------------------------------------

DT_DOCS(nrows, "Number of rows in the datatable")
static PyObject* get_nrows(DataTable_PyObject *self) {
  CATCH_EXCEPTIONS(
    return PyLong_FromLongLong(self->ref->nrows);
  );
}

DT_DOCS(ncols, "Number of columns in the datatable")
static PyObject* get_ncols(DataTable_PyObject *self) {
  CATCH_EXCEPTIONS(
    return PyLong_FromLongLong(self->ref->ncols);
  );
}

DT_DOCS(isview, "Is the datatable view or now?");
static PyObject* get_isview(DataTable_PyObject *self) {
  CATCH_EXCEPTIONS(
    return incref(self->ref->rowindex == NULL? Py_False : Py_True);
  );
}


DT_DOCS(ltypes, "List of column logical types")
static PyObject* get_ltypes(DataTable_PyObject *self)
{
  try {
    int64_t i = self->ref->ncols;
    PyObject *list = PyTuple_New((Py_ssize_t) i);
    if (list == NULL) return NULL;
    while (--i >= 0) {
      SType st = self->ref->columns[i]->stype();
      LType lt = stype_info[st].ltype;
      PyTuple_SET_ITEM(list, i, incref(py_ltype_objs[lt]));
    }
    return list;

  } catch (const std::exception& e) {
    PyErr_SetString(PyExc_RuntimeError, e.what());
    return NULL;
  }
}


DT_DOCS(stypes, "List of column storage types")
static PyObject* get_stypes(DataTable_PyObject *self)
{
  try {
    DataTable* dt = self->ref;
    int64_t i = dt->ncols;
    PyObject* list = PyTuple_New((Py_ssize_t) i);
    if (list == NULL) return NULL;
    while (--i >= 0) {
      SType st = dt->columns[i]->stype();
      PyTuple_SET_ITEM(list, i, incref(py_stype_objs[st]));
    }
    return list;

  } catch (const std::exception& e) {
    PyErr_SetString(PyExc_RuntimeError, e.what());
    return NULL;
  }
}


DT_DOCS(rowindex_type, "Type of the row index: 'slice' or 'array'")
static PyObject* get_rowindex_type(DataTable_PyObject *self)
{
  CATCH_EXCEPTIONS(
    if (self->ref->rowindex == NULL)
        return none();
    RowIndexType rit = self->ref->rowindex->type;
    return rit == RI_SLICE? incref(strRowIndexTypeSlice) :
           rit == RI_ARR32? incref(strRowIndexTypeArr32) :
           rit == RI_ARR64? incref(strRowIndexTypeArr64) : none();
  );
}


DT_DOCS(rowindex, "Row index of the view datatable, or None if this is not a view datatable")
static PyObject* get_rowindex(DataTable_PyObject *self)
{
  CATCH_EXCEPTIONS(
    RowIndex *ri = self->ref->rowindex;
    return ri? pyrowindex(self->ref->rowindex) : none();
  );
}


DT_DOCS(datatable_ptr, "Get pointer (converted to an int) to the wrapped DataTable object")
static PyObject* get_datatable_ptr(DataTable_PyObject *self)
{
  CATCH_EXCEPTIONS(
    return PyLong_FromLongLong((long long int)self->ref);
  );
}


/**
 * Return size of the referenced DataTable, but without the
 * `sizeof(DataTable_PyObject)`, which includes the size of the `self->ref`
 * pointer.
 */
DT_DOCS(alloc_size, "DataTable's internal size, in bytes")
static PyObject* get_alloc_size(DataTable_PyObject *self)
{
  CATCH_EXCEPTIONS(
    return PyLong_FromSize_t(self->ref->memory_footprint());
  );
}


DT_DOCS(window, "Retrieve datatable's data within a window");
static DataWindow_PyObject* meth_window(DataTable_PyObject *self, PyObject *args)
{
  CATCH_EXCEPTIONS(
    int64_t row0;
    int64_t row1;
    int64_t col0;
    int64_t col1;
    if (!PyArg_ParseTuple(args, "llll", &row0, &row1, &col0, &col1))
        return NULL;

    PyObject *nargs = Py_BuildValue("Ollll", self, row0, row1, col0, col1);
    PyObject *res = PyObject_CallObject((PyObject*) &DataWindow_PyType, nargs);
    Py_XDECREF(nargs);

    return (DataWindow_PyObject*) res;
  );
}



PyObject* pydatatable_assemble(UU, PyObject *args)
{
  CATCH_EXCEPTIONS(
    PyObject *arg1;
    ColumnSet_PyObject *pycols;
    if (!PyArg_ParseTuple(args, "OO!:datatable_assemble_view",
                          &arg1, &ColumnSet_PyType, &pycols))
        return nullptr;
    Column **columns = pycols->columns;
    pycols->columns = nullptr;
    RowIndex *rowindex = nullptr;
    if (arg1 == Py_None) {
        /* Don't do anything: rowindex should be NULL */
    } else {
        if (!PyObject_TypeCheck(arg1, &RowIndex_PyType)) {
            PyErr_Format(PyExc_TypeError, "Expected object of type RowIndex");
            return nullptr;
        }
        RowIndex_PyObject *pyri = (RowIndex_PyObject*) arg1;
        rowindex = pyri->ref;
        for (int64_t i = 0; columns[i] != nullptr; ++i) {
            Column* tmp = columns[i]->shallowcopy(rowindex);
            delete columns[i];
            columns[i] = tmp;
        }
    }
    return py(new DataTable(columns));
  );
}



PyObject* pydatatable_load(PyObject*, PyObject* args)
{
  CATCH_EXCEPTIONS(
    DataTable *colspec;
    int64_t nrows;
    const char* path;
    if (!PyArg_ParseTuple(args, "O&ns:datatable_load",
                          &dt_unwrap, &colspec, &nrows, &path))
        return NULL;
    return py(DataTable::load(colspec, nrows, path));
  );
}



/**
 * Check the DataTable for internal consistency. Returns True if the datatable
 * is faultless, or False if any problem is found. This function also accepts
 * a single optional argument -- `stream`. If this argument is given and any
 * errors were encountered, then the text of the errors will be written to this
 * stream object using `.write()` method. If this argument is not given, then
 * error messages will be printed to stdout. This argument has no effect if no
 * errors in the file were found.
 */
DT_DOCS(check, "Check and repair the datatable");
static PyObject* meth_check(DataTable_PyObject *self, PyObject *args)
{
  CATCH_EXCEPTIONS(
    DataTable *dt = self->ref;
    PyObject *stream = NULL;

    if (!PyArg_ParseTuple(args, "|O:check", &stream)) return NULL;

    IntegrityCheckContext icc(200);
    dt->verify_integrity(icc);
    if (icc.has_errors()) {
        if (stream) {
            PyObject *ret = PyObject_CallMethod(stream, "write", "s",
                                                icc.errors().str().c_str());
            if (ret == NULL) return NULL;
            Py_DECREF(ret);
        }
        else {
            std::cout << icc.errors().str();
        }
    }
    return incref(icc.has_errors()? Py_False : Py_True);
  );
}



DT_DOCS(column, "Get the requested column in the datatable")
static PyObject* meth_column(DataTable_PyObject *self, PyObject *args)
{
  CATCH_EXCEPTIONS(
    DataTable *dt = self->ref;
    int64_t colidx;
    if (!PyArg_ParseTuple(args, "l:column", &colidx))
        return NULL;
    if (colidx < -dt->ncols || colidx >= dt->ncols) {
        PyErr_Format(PyExc_ValueError, "Invalid column index %lld", colidx);
        return NULL;
    }
    if (colidx < 0) colidx += dt->ncols;
    Column_PyObject *pycol =
        pycolumn_from_column(dt->columns[colidx], self, colidx);
    return (PyObject*) pycol;
  );
}



DT_DOCS(delete_columns, "Remove the specified list of columns from the datatable")
static PyObject* meth_delete_columns(DataTable_PyObject *self, PyObject *args)
{
  CATCH_EXCEPTIONS(
    DataTable *dt = self->ref;
    PyObject *list;
    if (!PyArg_ParseTuple(args, "O!:delete_columns", &PyList_Type, &list))
        return NULL;

    int ncols = (int) PyList_Size(list);
    int *cols_to_remove = NULL;
    dtmalloc(cols_to_remove, int, ncols);
    for (int i = 0; i < ncols; i++) {
        PyObject *item = PyList_GET_ITEM(list, i);
        cols_to_remove[i] = (int) PyLong_AsLong(item);
    }
    dt->delete_columns(cols_to_remove, ncols);

    dtfree(cols_to_remove);
    return none();
  );
}



DT_DOCS(rbind, "Append rows of other datatables to the current")
static PyObject* meth_rbind(DataTable_PyObject *self, PyObject *args)
{
  CATCH_EXCEPTIONS(
    DataTable *dt = self->ref;
    int final_ncols;
    PyObject *list;
    if (!PyArg_ParseTuple(args, "iO!:delete_columns",
                          &final_ncols, &PyList_Type, &list))
        return NULL;

    int ndts = (int) PyList_Size(list);
    DataTable **dts = NULL;
    dtmalloc(dts, DataTable*, ndts);
    int **cols_to_append = NULL;
    dtmalloc(cols_to_append, int*, final_ncols);
    for (int i = 0; i < final_ncols; i++) {
        dtmalloc(cols_to_append[i], int, ndts);
    }
    for (int i = 0; i < ndts; i++) {
        PyObject *item = PyList_GET_ITEM(list, i);
        DataTable *dti;
        PyObject *colslist;
        if (!PyArg_ParseTuple(item, "O&O!",
                              &dt_unwrap, &dti, &PyList_Type, &colslist))
            return NULL;
        int ncolsi = (int) PyList_Size(colslist);
        int j = 0;
        for (; j < ncolsi; j++) {
            PyObject *itemj = PyList_GET_ITEM(colslist, j);
            cols_to_append[j][i] = (itemj == Py_None)? -1
                                   : (int) PyLong_AsLong(itemj);
        }
        for (; j < final_ncols; j++) {
            cols_to_append[j][i] = -1;
        }
        dts[i] = dti;
    }
    try {
        DataTable *ret = dt->rbind( dts, cols_to_append, ndts, final_ncols);
        if (ret == NULL) return NULL;
    } catch (const std::exception& e) {
        PyErr_SetString(PyExc_RuntimeError, e.what());
        return NULL;
    }

    dtfree(cols_to_append);
    dtfree(dts);
    return none();
  );
}


DT_DOCS(cbind, "Append columns of other datatables to the current")
static PyObject* meth_cbind(DataTable_PyObject *self, PyObject *args)
{
  CATCH_EXCEPTIONS(
    PyObject *pydts;
    if (!PyArg_ParseTuple(args, "O!:cbind",
                          &PyList_Type, &pydts)) return NULL;

    DataTable *dt = self->ref;
    int ndts = (int) PyList_Size(pydts);
    DataTable **dts = NULL;
    dtmalloc(dts, DataTable*, ndts);
    for (int i = 0; i < ndts; i++) {
        PyObject *elem = PyList_GET_ITEM(pydts, i);
        if (!PyObject_TypeCheck(elem, &DataTable_PyType)) {
            PyErr_Format(PyExc_ValueError,
                "Element %d of the array is not a DataTable object", i);
            return NULL;
        }
        dts[i] = ((DataTable_PyObject*) elem)->ref;
    }
    DataTable *ret = dt->cbind( dts, ndts);
    if (ret == NULL) return NULL;

    dtfree(dts);
    return none();
  );
}



DT_DOCS(sort, "Sort datatable according to a column")
static PyObject* meth_sort(DataTable_PyObject *self, PyObject *args)
{
  CATCH_EXCEPTIONS(
    DataTable *dt = self->ref;
    int idx;
    if (!PyArg_ParseTuple(args, "i:sort", &idx)) return NULL;

    Column *col = dt->columns[idx];
    RowIndex *ri = col->sort();
    return pyrowindex(ri);
  );
}

#define DT_METH_GET_STAT(STAT, DOCSTRING) \
  DT_DOCS(get_ ## STAT , DOCSTRING) \
  static PyObject* meth_get_## STAT (DataTable_PyObject *self, PyObject *args) \
  { \
    CATCH_EXCEPTIONS( \
      if (!PyArg_ParseTuple(args, "")) return NULL; \
      return py(self->ref-> STAT ## _datatable()); \
    ); \
  }

DT_METH_GET_STAT(min, "Get the minimum for each column in the datatable")
DT_METH_GET_STAT(max, "Get the maximum for each column in the datatable")
DT_METH_GET_STAT(sum, "Get the sum for each column in the datatable")
DT_METH_GET_STAT(mean, "Get the mean for each column in the datatable")
DT_METH_GET_STAT(sd, "Get the standard deviation for each column in the datatable")
DT_METH_GET_STAT(countna, "Get the NA count for each column in the datatable")

#undef DT_METH_GET_STAT

DT_DOCS(materialize, "")
static PyObject* meth_materialize(DataTable_PyObject *self, PyObject *args)
{
  CATCH_EXCEPTIONS(
    if (!PyArg_ParseTuple(args, "")) return NULL;

    DataTable *dt = self->ref;
    if (dt->rowindex == NULL) {
        PyErr_Format(PyExc_ValueError, "Only a view can be materialized");
        return NULL;
    }

    Column **cols = NULL;
    dtmalloc(cols, Column*, dt->ncols + 1);
    for (int64_t i = 0; i < dt->ncols; ++i) {
        cols[i] = dt->columns[i]->shallowcopy();
        if (cols[i] == NULL) return NULL;
        cols[i]->reify();
    }
    cols[dt->ncols] = NULL;

    DataTable *newdt = new DataTable(cols);
    return py(newdt);
  );
}


DT_DOCS(apply_na_mask, "")
static PyObject* meth_apply_na_mask(DataTable_PyObject *self, PyObject *args)
{
  CATCH_EXCEPTIONS(
    DataTable *dt = self->ref;
    DataTable *mask = NULL;
    if (!PyArg_ParseTuple(args, "O&", &dt_unwrap, &mask)) return NULL;

    dt->apply_na_mask(mask);
    return none();
  );
}


DT_DOCS(use_stype_for_buffers, "")
static PyObject* meth_use_stype_for_buffers(
    DataTable_PyObject* self, PyObject* args
) {
  CATCH_EXCEPTIONS(
    int st = 0;
    if (!PyArg_ParseTuple(args, "|i:use_stype_for_buffers", &st)) return NULL;
    self->use_stype_for_buffers = static_cast<SType>(st);
    return none();
  );
}


/**
 * Deallocator function, called when the object is being garbage-collected.
 */
static void _dealloc_(DataTable_PyObject *self)
{
    delete self->ref;
    Py_TYPE(self)->tp_free((PyObject*)self);
}



//======================================================================================================================
// DataTable type definition
//======================================================================================================================

static PyMethodDef datatable_methods[] = {
    DT_METHOD1(window),
    DT_METHOD1(check),
    DT_METHOD1(column),
    DT_METHOD1(delete_columns),
    DT_METHOD1(rbind),
    DT_METHOD1(cbind),
    DT_METHOD1(sort),
    DT_METHOD1(get_min),
    DT_METHOD1(get_max),
    DT_METHOD1(get_sum),
    DT_METHOD1(get_mean),
    DT_METHOD1(get_sd),
    DT_METHOD1(get_countna),
    DT_METHOD1(materialize),
    DT_METHOD1(apply_na_mask),
    DT_METHOD1(use_stype_for_buffers),
    {NULL, NULL, 0, NULL}           /* sentinel */
};

static PyGetSetDef datatable_getseters[] = {
    DT_GETSETTER(nrows),
    DT_GETSETTER(ncols),
    DT_GETSETTER(ltypes),
    DT_GETSETTER(stypes),
    DT_GETSETTER(isview),
    DT_GETSETTER(rowindex),
    DT_GETSETTER(rowindex_type),
    DT_GETSETTER(datatable_ptr),
    DT_GETSETTER(alloc_size),
    {NULL, NULL, NULL, NULL, NULL}  /* sentinel */
};

PyTypeObject DataTable_PyType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "_datatable.DataTable",             /* tp_name */
    sizeof(DataTable_PyObject),         /* tp_basicsize */
    0,                                  /* tp_itemsize */
    (destructor)_dealloc_,              /* tp_dealloc */
    0,                                  /* tp_print */
    0,                                  /* tp_getattr */
    0,                                  /* tp_setattr */
    0,                                  /* tp_compare */
    0,                                  /* tp_repr */
    0,                                  /* tp_as_number */
    0,                                  /* tp_as_sequence */
    0,                                  /* tp_as_mapping */
    0,                                  /* tp_hash  */
    0,                                  /* tp_call */
    0,                                  /* tp_str */
    0,                                  /* tp_getattro */
    0,                                  /* tp_setattro */
    &datatable_as_buffer,               /* tp_as_buffer;  see py_buffers.c */
    Py_TPFLAGS_DEFAULT,                 /* tp_flags */
    "DataTable object",                 /* tp_doc */
    0,                                  /* tp_traverse */
    0,                                  /* tp_clear */
    0,                                  /* tp_richcompare */
    0,                                  /* tp_weaklistoffset */
    0,                                  /* tp_iter */
    0,                                  /* tp_iternext */
    datatable_methods,                  /* tp_methods */
    0,                                  /* tp_members */
    datatable_getseters,                /* tp_getset */
    0,                                  /* tp_base */
    0,                                  /* tp_dict */
    0,                                  /* tp_descr_get */
    0,                                  /* tp_descr_set */
    0,                                  /* tp_dictoffset */
    0,                                  /* tp_init */
    0,                                  /* tp_alloc */
    0,                                  /* tp_new */
    0,                                  /* tp_free */
    0,                                  /* tp_is_gc */
    0,                                  /* tp_bases */
    0,                                  /* tp_mro */
    0,                                  /* tp_cache */
    0,                                  /* tp_subclasses */
    0,                                  /* tp_weaklist */
    0,                                  /* tp_del */
    0,                                  /* tp_version_tag */
    0,                                  /* tp_finalize */
};


int init_py_datatable(PyObject *module) {
    init_stats();
    // Register DataTable_PyType on the module
    DataTable_PyType.tp_new = PyType_GenericNew;
    if (PyType_Ready(&DataTable_PyType) < 0) return 0;
    Py_INCREF(&DataTable_PyType);
    PyModule_AddObject(module, "DataTable", (PyObject*) &DataTable_PyType);

    strRowIndexTypeArr32 = PyUnicode_FromString("arr32");
    strRowIndexTypeArr64 = PyUnicode_FromString("arr64");
    strRowIndexTypeSlice = PyUnicode_FromString("slice");
    return 1;
}
