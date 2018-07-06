//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#define dt_PY_DATATABLE_cc
#include "py_datatable.h"
#include <exception>
#include <iostream>
#include <vector>
#include "datatable.h"
#include "py_column.h"
#include "py_columnset.h"
#include "py_datawindow.h"
#include "py_groupby.h"
#include "py_rowindex.h"
#include "py_types.h"
#include "py_utils.h"
#include "python/string.h"


namespace pydatatable
{

// Forward declarations
static PyObject* strRowIndexTypeArr32;
static PyObject* strRowIndexTypeArr64;
static PyObject* strRowIndexTypeSlice;


/**
 * Create a new `pydatatable::obj` by wrapping the provided DataTable `dt`.
 * If `dt` is nullptr then this function also returns nullptr.
 */
PyObject* wrap(DataTable* dt)
{
  if (!dt) return nullptr;
  PyObject* pytype = reinterpret_cast<PyObject*>(&pydatatable::type);
  PyObject* pydt = PyObject_CallObject(pytype, nullptr);
  if (pydt) {
    auto pypydt = reinterpret_cast<pydatatable::obj*>(pydt);
    pypydt->ref = dt;
    pypydt->use_stype_for_buffers = ST_VOID;
  }
  return pydt;
}


/**
 * Attempt to extract a DataTable from the given PyObject, iff it is a
 * pydatatable::obj. On success, the reference to the DataTable is stored in
 * the `address` and 1 is returned. On failure, returns 0.
 *
 * This function does not attempt to DECREF the source PyObject.
 */
int unwrap(PyObject* object, DataTable** address) {
  if (!object) return 0;
  if (!PyObject_TypeCheck(object, &pydatatable::type)) {
    PyErr_SetString(PyExc_TypeError, "Expected object of type DataTable");
    return 0;
  }
  *address = static_cast<pydatatable::obj*>(object)->ref;
  return 1;
}



//==============================================================================
// Generic Python API
//==============================================================================

PyObject* datatable_load(PyObject*, PyObject* args) {
  DataTable* colspec;
  int64_t nrows;
  const char* path;
  int recode;
  if (!PyArg_ParseTuple(args, "O&nsi:datatable_load",
                        &unwrap, &colspec, &nrows, &path, &recode))
    return nullptr;
  return wrap(DataTable::load(colspec, nrows, path, recode));
}


PyObject* open_jay(PyObject*, PyObject* args) {
  PyObject* arg1;
  if (!PyArg_ParseTuple(args, "O:open_jay", &arg1)) return nullptr;
  std::string filename = PyObj(arg1).as_string();

  std::vector<std::string> colnames;
  DataTable* dt = DataTable::open_jay(filename, colnames);
  PyObject* pydt = wrap(dt);

  PyyList collist(colnames.size());
  for (size_t i = 0; i < colnames.size(); ++i) {
    collist[i] = PyyString(colnames[i]);
  }
  PyObject* pylist = collist.release();

  return Py_BuildValue("OO", pydt, pylist);
}



//==============================================================================
// PyDatatable getters/setters
//==============================================================================

PyObject* get_nrows(obj* self) {
  return PyLong_FromLongLong(self->ref->nrows);
}

PyObject* get_ncols(obj* self) {
  return PyLong_FromLongLong(self->ref->ncols);
}

PyObject* get_isview(obj* self) {
  return incref(self->ref->rowindex.isabsent()? Py_False : Py_True);
}


PyObject* get_ltypes(obj* self) {
  int64_t i = self->ref->ncols;
  PyObject* list = PyTuple_New(i);
  if (list == nullptr) return nullptr;
  while (--i >= 0) {
    SType st = self->ref->columns[i]->stype();
    LType lt = stype_info[st].ltype;
    PyTuple_SET_ITEM(list, i, incref(py_ltype_objs[lt]));
  }
  return list;
}


PyObject* get_stypes(obj* self) {
  DataTable* dt = self->ref;
  int64_t i = dt->ncols;
  PyObject* list = PyTuple_New(i);
  if (list == nullptr) return nullptr;
  while (--i >= 0) {
    SType st = dt->columns[i]->stype();
    PyTuple_SET_ITEM(list, i, incref(py_stype_objs[st]));
  }
  return list;
}


PyObject* get_rowindex_type(obj* self) {
  RowIndex& ri = self->ref->rowindex;
  return ri.isabsent()? none() :
         ri.isslice()? incref(strRowIndexTypeSlice) :
         ri.isarr32()? incref(strRowIndexTypeArr32) :
         ri.isarr64()? incref(strRowIndexTypeArr64) : none();
}


PyObject* get_rowindex(obj* self) {
  RowIndex& ri = self->ref->rowindex;
  return ri.isabsent()? none() : pyrowindex::wrap(ri);
}


PyObject* get_groupby(obj* self) {
  Groupby& gb = self->ref->groupby;
  return gb.ngroups()? pygroupby::wrap(gb) : none();
}


int set_groupby(obj* self, PyObject* value) {
  Groupby* gb = pygroupby::unwrap(value);
  self->ref->replace_groupby(gb? *gb : Groupby());
  return 0;
}


PyObject* get_datatable_ptr(obj* self) {
  return PyLong_FromLongLong(reinterpret_cast<long long int>(self->ref));
}


/**
 * Return size of the referenced DataTable, but without the
 * `sizeof(pydatatable::obj)`, which includes the size of the `self->ref`
 * pointer.
 */
PyObject* get_alloc_size(obj* self) {
  return PyLong_FromSize_t(self->ref->memory_footprint());
}



//==============================================================================
// PyDatatable methods
//==============================================================================

PyObject* window(obj* self, PyObject* args) {
  int64_t row0, row1, col0, col1;
  if (!PyArg_ParseTuple(args, "llll", &row0, &row1, &col0, &col1))
    return nullptr;

  PyObject* dwtype = reinterpret_cast<PyObject*>(&pydatawindow::type);
  PyObject* nargs = Py_BuildValue("Ollll", self, row0, row1, col0, col1);
  PyObject* res = PyObject_CallObject(dwtype, nargs);
  Py_XDECREF(nargs);

  return res;
}


PyObject* to_scalar(obj* self, PyObject*) {
  DataTable* dt = self->ref;
  if (dt->ncols == 1 && dt->nrows == 1) {
    Column* col = dt->columns[0];
    int64_t i = col->rowindex().nth(0);
    auto f = py_stype_formatters[col->stype()];
    return f(col, i);
  } else {
    throw ValueError() << ".scalar() method cannot be applied to a Frame with "
      "shape `[" << dt->nrows << " x " << dt->ncols << "]`";
  }
}


PyObject* check(obj* self, PyObject*) {
  DataTable* dt = self->ref;
  dt->verify_integrity();
  return none();
}


PyObject* column(obj* self, PyObject* args) {
  DataTable* dt = self->ref;
  int64_t colidx;
  if (!PyArg_ParseTuple(args, "l:column", &colidx))
    return nullptr;
  if (colidx < -dt->ncols || colidx >= dt->ncols) {
    PyErr_Format(PyExc_ValueError, "Invalid column index %lld", colidx);
    return nullptr;
  }
  if (colidx < 0) colidx += dt->ncols;
  pycolumn::obj* pycol =
      pycolumn::from_column(dt->columns[colidx], self, colidx);
  return pycol;
}



PyObject* delete_columns(obj* self, PyObject* args) {
  DataTable* dt = self->ref;
  PyObject* list;
  if (!PyArg_ParseTuple(args, "O!:delete_columns", &PyList_Type, &list))
    return nullptr;

  int64_t ncols = static_cast<int64_t>(PyList_Size(list));
  int* cols_to_remove = dt::amalloc<int>(ncols);
  for (int64_t i = 0; i < ncols; i++) {
    PyObject* item = PyList_GET_ITEM(list, i);
    cols_to_remove[i] = static_cast<int>(PyLong_AsLong(item));
  }
  dt->delete_columns(cols_to_remove, ncols);

  dt::free(cols_to_remove);
  Py_RETURN_NONE;
}



PyObject* resize_rows(obj* self, PyObject* args) {
  DataTable* dt = self->ref;
  int64_t new_nrows;
  if (!PyArg_ParseTuple(args, "l:resize_rows", &new_nrows)) return nullptr;

  dt->resize_rows(new_nrows);
  Py_RETURN_NONE;
}



PyObject* replace_rowindex(obj* self, PyObject* args) {
  DataTable* dt = self->ref;
  PyObject* arg1;
  if (!PyArg_ParseTuple(args, "O:replace_rowindex", &arg1))
    return nullptr;
  RowIndex newri = PyObj(arg1).as_rowindex();

  dt->replace_rowindex(newri);
  Py_RETURN_NONE;
}



PyObject* replace_column_slice(obj* self, PyObject* args) {
  DataTable* dt = self->ref;
  int64_t start, count, step;
  PyObject *arg4, *arg5;
  if (!PyArg_ParseTuple(args, "lllOO:replace_column_slice",
                        &start, &count, &step, &arg4, &arg5)) return nullptr;
  RowIndex rows_ri = PyObj(arg4).as_rowindex();
  DataTable* repl = PyObj(arg5).as_datatable();
  int64_t rrows = repl->nrows;
  int64_t rcols = repl->ncols;
  int64_t rrows2 = rows_ri? rows_ri.length() : dt->nrows;

  if (!check_slice_triple(start, count, step, dt->ncols - 1)) {
    throw ValueError() << "Invalid slice " << start << "/" << count
                       << "/" << step << " for a Frame with " << dt->ncols
                       << " columns";
  }
  bool ok = (rrows == rrows2 || rrows == 1) && (rcols == count || rcols == 1);
  if (!ok) {
    throw ValueError() << "Invalid replacement Frame: expected [" <<
      rrows2 << " x " << count << "], but received [" << rrows <<
      " x " << rcols << "]";
  }

  dt->reify();  // noop if `dt` is not a view
  repl->reify();

  for (int64_t i = 0; i < count; ++i) {
    int64_t j = start + i * step;
    Column* replcol = repl->columns[i % rcols];
    if (rows_ri) {
      dt->columns[j]->replace_values(rows_ri, replcol);
    } else {
      delete dt->columns[j];
      dt->columns[j] = replcol->shallowcopy();
    }
  }
  Py_RETURN_NONE;
}


PyObject* replace_column_array(obj* self, PyObject* args) {
  DataTable* dt = self->ref;
  PyObject *arg1, *arg2, *arg3;
  if (!PyArg_ParseTuple(args, "OOO:replace_column_array", &arg1, &arg2, &arg3))
      return nullptr;
  PyyList cols(arg1);
  RowIndex rows_ri = PyObj(arg2).as_rowindex();
  DataTable* repl = PyObj(arg3).as_datatable();
  int64_t rrows = repl->nrows;
  size_t rcols = static_cast<size_t>(repl->ncols);
  int64_t rrows2 = rows_ri? rows_ri.length() : dt->nrows;

  bool ok = (rrows == rrows2 || rrows == 1) &&
            (rcols == cols.size() || rcols == 1);
  if (!ok) {
    throw ValueError() << "Invalid replacement Frame: expected [" <<
      rrows2 << " x " << cols.size() << "], but received [" << rrows <<
      " x " << rcols << "]";
  }

  dt->reify();
  repl->reify();

  int64_t num_new_cols = 0;
  for (size_t i = 0; i < cols.size(); ++i) {
    PyObj item = cols[i];
    int64_t j = item.as_int64();
    num_new_cols += (j == -1);
    if (j < -1 || j >= dt->ncols) {
      throw ValueError() << "Invalid index for a replacement column: " << j;
    }
  }
  if (num_new_cols) {
    if (rows_ri) {
      throw ValueError() << "Cannot assign to column(s) that are outside of "
                            "the Frame: " << rows_ri;
    }
    size_t newsize = static_cast<size_t>(dt->ncols + num_new_cols + 1)
                     * sizeof(Column*);
    dt->columns = static_cast<Column**>(realloc(dt->columns, newsize));
  }
  for (size_t i = 0; i < cols.size(); ++i) {
    PyObj item = cols[i];
    int64_t j = item.as_int64();
    Column* replcol = repl->columns[i % rcols];
    if (rows_ri) {
      dt->columns[j]->replace_values(rows_ri, replcol);
    } else {
      if (j == -1) {
        j = dt->ncols++;
      } else {
        delete dt->columns[j];
      }
      dt->columns[j] = replcol->shallowcopy();
    }
  }
  dt->columns[dt->ncols] = nullptr;

  Py_RETURN_NONE;
}


PyObject* rbind(obj* self, PyObject* args) {
  DataTable* dt = self->ref;
  int64_t final_ncols;
  PyObject* list;
  if (!PyArg_ParseTuple(args, "lO!:delete_columns",
                        &final_ncols, &PyList_Type, &list))
    return nullptr;

  int64_t ndts = static_cast<int64_t>(PyList_Size(list));
  DataTable** dts = dt::amalloc<DataTable*>(ndts);
  int** cols_to_append = dt::amalloc<int*>(final_ncols);
  for (int64_t i = 0; i < final_ncols; i++) {
    cols_to_append[i] = dt::amalloc<int>(ndts);
  }
  for (int i = 0; i < ndts; i++) {
    PyObject* item = PyList_GET_ITEM(list, i);
    DataTable* dti;
    PyObject* colslist;
    if (!PyArg_ParseTuple(item, "O&O",
                          &unwrap, &dti, &colslist))
      return nullptr;
    int64_t j = 0;
    if (colslist == Py_None) {
      int64_t ncolsi = dti->ncols;
      for (; j < ncolsi; ++j) {
        cols_to_append[j][i] = static_cast<int>(j);
      }
    } else {
      int64_t ncolsi = PyList_Size(colslist);
      for (; j < ncolsi; ++j) {
        PyObject* itemj = PyList_GET_ITEM(colslist, j);
        cols_to_append[j][i] = (itemj == Py_None)? -1
                               : static_cast<int>(PyLong_AsLong(itemj));
      }
    }
    for (; j < final_ncols; ++j) {
      cols_to_append[j][i] = -1;
    }
    dts[i] = dti;
  }

  dt->rbind(dts, cols_to_append, ndts, final_ncols);

  dt::free(cols_to_append);
  dt::free(dts);
  Py_RETURN_NONE;
}


PyObject* cbind(obj* self, PyObject* args) {
  PyObject* pydts;
  if (!PyArg_ParseTuple(args, "O!:cbind",
                        &PyList_Type, &pydts)) return nullptr;

  DataTable* dt = self->ref;
  int64_t ndts = static_cast<int64_t>(PyList_Size(pydts));
  DataTable** dts = dt::amalloc<DataTable*>(ndts);
  for (int64_t i = 0; i < ndts; i++) {
    PyObject* elem = PyList_GET_ITEM(pydts, i);
    if (!PyObject_TypeCheck(elem, &type)) {
      PyErr_Format(PyExc_ValueError,
          "Element %d of the array is not a DataTable object", i);
      return nullptr;
    }
    dts[i] = static_cast<pydatatable::obj*>(elem)->ref;
  }
  DataTable* ret = dt->cbind( dts, ndts);
  if (ret == nullptr) return nullptr;

  dt::free(dts);
  Py_RETURN_NONE;
}



PyObject* sort(obj* self, PyObject* args) {
  DataTable* dt = self->ref;
  int idx = 0;
  int make_groups = 0;
  if (!PyArg_ParseTuple(args, "i|i:sort", &idx, &make_groups))
    return nullptr;

  arr32_t cols(1);
  cols[0] = idx;
  Groupby grpby;
  RowIndex ri = dt->sortby(cols, make_groups? &grpby : nullptr);
  // return pyrowindex::wrap(ri);
  return Py_BuildValue("NN", pyrowindex::wrap(ri),
                             make_groups? pygroupby::wrap(grpby) : none());
}



PyObject* get_min    (obj* self, PyObject*) { return wrap(self->ref->min_datatable()); }
PyObject* get_max    (obj* self, PyObject*) { return wrap(self->ref->max_datatable()); }
PyObject* get_mode   (obj* self, PyObject*) { return wrap(self->ref->mode_datatable()); }
PyObject* get_mean   (obj* self, PyObject*) { return wrap(self->ref->mean_datatable()); }
PyObject* get_sd     (obj* self, PyObject*) { return wrap(self->ref->sd_datatable()); }
PyObject* get_skew   (obj* self, PyObject*) { return wrap(self->ref->skew_datatable()); }
PyObject* get_kurt   (obj* self, PyObject*) { return wrap(self->ref->kurt_datatable()); }
PyObject* get_sum    (obj* self, PyObject*) { return wrap(self->ref->sum_datatable()); }
PyObject* get_countna(obj* self, PyObject*) { return wrap(self->ref->countna_datatable()); }
PyObject* get_nunique(obj* self, PyObject*) { return wrap(self->ref->nunique_datatable()); }
PyObject* get_nmodal (obj* self, PyObject*) { return wrap(self->ref->nmodal_datatable()); }

typedef PyObject* (Column::*scalarstatfn)() const;
static PyObject* _scalar_stat(DataTable* dt, scalarstatfn f) {
  if (dt->ncols != 1) throw ValueError() << "This method can only be applied to a 1-column Frame";
  return (dt->columns[0]->*f)();
}

PyObject* countna1(obj* self, PyObject*) { return _scalar_stat(self->ref, &Column::countna_pyscalar); }
PyObject* nunique1(obj* self, PyObject*) { return _scalar_stat(self->ref, &Column::nunique_pyscalar); }
PyObject* nmodal1 (obj* self, PyObject*) { return _scalar_stat(self->ref, &Column::nmodal_pyscalar); }
PyObject* min1    (obj* self, PyObject*) { return _scalar_stat(self->ref, &Column::min_pyscalar); }
PyObject* max1    (obj* self, PyObject*) { return _scalar_stat(self->ref, &Column::max_pyscalar); }
PyObject* mode1   (obj* self, PyObject*) { return _scalar_stat(self->ref, &Column::mode_pyscalar); }
PyObject* mean1   (obj* self, PyObject*) { return _scalar_stat(self->ref, &Column::mean_pyscalar); }
PyObject* sd1     (obj* self, PyObject*) { return _scalar_stat(self->ref, &Column::sd_pyscalar); }
PyObject* sum1    (obj* self, PyObject*) { return _scalar_stat(self->ref, &Column::sum_pyscalar); }



PyObject* materialize(obj* self, PyObject*) {
  DataTable* dt = self->ref;
  if (dt->rowindex.isabsent()) {
    PyErr_Format(PyExc_ValueError, "Only a view can be materialized");
    return nullptr;
  }

  Column** cols = dt::amalloc<Column*>(dt->ncols + 1);
  for (int64_t i = 0; i < dt->ncols; ++i) {
    cols[i] = dt->columns[i]->shallowcopy();
    if (cols[i] == nullptr) return nullptr;
    cols[i]->reify();
  }
  cols[dt->ncols] = nullptr;

  DataTable* newdt = new DataTable(cols);
  return wrap(newdt);
}


PyObject* apply_na_mask(obj* self, PyObject* args) {
  DataTable* dt = self->ref;
  DataTable* mask = nullptr;
  if (!PyArg_ParseTuple(args, "O&", &unwrap, &mask)) return nullptr;

  dt->apply_na_mask(mask);
  Py_RETURN_NONE;
}


PyObject* use_stype_for_buffers(obj* self, PyObject* args) {
  int st = 0;
  if (!PyArg_ParseTuple(args, "|i:use_stype_for_buffers", &st))
    return nullptr;
  self->use_stype_for_buffers = static_cast<SType>(st);
  Py_RETURN_NONE;
}


PyObject* save_jay(obj* self, PyObject* args) {
  DataTable* dt = self->ref;
  PyObject* arg1, *arg2;
  if (!PyArg_ParseTuple(args, "OO:save_jay", &arg1, &arg2)) return nullptr;
  std::string filename = PyObj(arg1).as_string();
  std::vector<std::string> colnames = PyObj(arg2).as_stringlist();

  if (colnames.size() != static_cast<size_t>(dt->ncols)) {
    throw ValueError()
      << "The list of column names has wrong length: " << colnames.size();
  }

  dt->save_jay(filename, colnames);
  Py_RETURN_NONE;
}


static void dealloc(obj* self) {
  delete self->ref;
  Py_TYPE(self)->tp_free(self);
}



//==============================================================================
// DataTable type definition
//==============================================================================

static PyMethodDef datatable_methods[] = {
  METHODv(window),
  METHOD0(to_scalar),
  METHOD0(check),
  METHODv(column),
  METHODv(delete_columns),
  METHODv(resize_rows),
  METHODv(replace_rowindex),
  METHODv(replace_column_slice),
  METHODv(replace_column_array),
  METHODv(rbind),
  METHODv(cbind),
  METHODv(sort),
  METHOD0(get_min),
  METHOD0(get_max),
  METHOD0(get_mode),
  METHOD0(get_sum),
  METHOD0(get_mean),
  METHOD0(get_sd),
  METHOD0(get_skew),
  METHOD0(get_kurt),
  METHOD0(get_countna),
  METHOD0(get_nunique),
  METHOD0(get_nmodal),
  METHOD0(countna1),
  METHOD0(nunique1),
  METHOD0(nmodal1),
  METHOD0(min1),
  METHOD0(max1),
  METHOD0(mode1),
  METHOD0(sum1),
  METHOD0(mean1),
  METHOD0(sd1),
  METHOD0(materialize),
  METHODv(apply_na_mask),
  METHODv(use_stype_for_buffers),
  METHODv(save_jay),
  {nullptr, nullptr, 0, nullptr}           /* sentinel */
};

static PyGetSetDef datatable_getseters[] = {
  GETTER(nrows),
  GETTER(ncols),
  GETTER(ltypes),
  GETTER(stypes),
  GETTER(isview),
  GETTER(rowindex),
  GETSET(groupby),
  GETTER(rowindex_type),
  GETTER(datatable_ptr),
  GETTER(alloc_size),
  {nullptr, nullptr, nullptr, nullptr, nullptr}  /* sentinel */
};

PyTypeObject type = {
  PyVarObject_HEAD_INIT(nullptr, 0)
  cls_name,                           /* tp_name */
  sizeof(pydatatable::obj),           /* tp_basicsize */
  0,                                  /* tp_itemsize */
  DESTRUCTOR,                         /* tp_dealloc */
  nullptr,                            /* tp_print */
  nullptr,                            /* tp_getattr */
  nullptr,                            /* tp_setattr */
  nullptr,                            /* tp_compare */
  nullptr,                            /* tp_repr */
  nullptr,                            /* tp_as_number */
  nullptr,                            /* tp_as_sequence */
  nullptr,                            /* tp_as_mapping */
  nullptr,                            /* tp_hash  */
  nullptr,                            /* tp_call */
  nullptr,                            /* tp_str */
  nullptr,                            /* tp_getattro */
  nullptr,                            /* tp_setattro */
  &pydatatable::as_buffer,            /* tp_as_buffer;  see py_buffers.cc */
  Py_TPFLAGS_DEFAULT,                 /* tp_flags */
  cls_doc,                            /* tp_doc */
  nullptr,                            /* tp_traverse */
  nullptr,                            /* tp_clear */
  nullptr,                            /* tp_richcompare */
  0,                                  /* tp_weaklistoffset */
  nullptr,                            /* tp_iter */
  nullptr,                            /* tp_iternext */
  datatable_methods,                  /* tp_methods */
  nullptr,                            /* tp_members */
  datatable_getseters,                /* tp_getset */
  nullptr,                            /* tp_base */
  nullptr,                            /* tp_dict */
  nullptr,                            /* tp_descr_get */
  nullptr,                            /* tp_descr_set */
  0,                                  /* tp_dictoffset */
  nullptr,                            /* tp_init */
  nullptr,                            /* tp_alloc */
  nullptr,                            /* tp_new */
  nullptr,                            /* tp_free */
  nullptr,                            /* tp_is_gc */
  nullptr,                            /* tp_bases */
  nullptr,                            /* tp_mro */
  nullptr,                            /* tp_cache */
  nullptr,                            /* tp_subclasses */
  nullptr,                            /* tp_weaklist */
  nullptr,                            /* tp_del */
  0,                                  /* tp_version_tag */
  nullptr,                            /* tp_finalize */
};


int static_init(PyObject* module) {
  // Register type on the module
  pydatatable::type.tp_new = PyType_GenericNew;
  if (PyType_Ready(&pydatatable::type) < 0) return 0;
  PyObject* typeobj = reinterpret_cast<PyObject*>(&type);
  Py_INCREF(typeobj);
  PyModule_AddObject(module, "DataTable", typeobj);

  strRowIndexTypeArr32 = PyUnicode_FromString("arr32");
  strRowIndexTypeArr64 = PyUnicode_FromString("arr64");
  strRowIndexTypeSlice = PyUnicode_FromString("slice");
  return 1;
}


};  // namespace pydatatable
