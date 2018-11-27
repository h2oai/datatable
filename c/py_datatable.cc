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
#include "frame/py_frame.h"
#include "py_column.h"
#include "py_columnset.h"
#include "py_datawindow.h"
#include "py_groupby.h"
#include "py_rowindex.h"
#include "py_types.h"
#include "py_utils.h"
#include "python/int.h"
#include "python/list.h"
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
    pypydt->_frame = nullptr;
    pypydt->use_stype_for_buffers = SType::VOID;
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
  size_t nrows;
  const char* path;
  int recode;
  PyObject* names;
  if (!PyArg_ParseTuple(args, "O&nsiO:datatable_load",
                        &unwrap, &colspec, &nrows, &path, &recode, &names))
    return nullptr;

  DataTable* dt = DataTable::load(colspec, nrows, path, recode);
  py::Frame* frame = py::Frame::from_datatable(dt);
  frame->set_names(py::robj(names));
  return frame;
}


PyObject* open_jay(PyObject*, PyObject* args) {
  PyObject* arg1;
  if (!PyArg_ParseTuple(args, "O:open_jay", &arg1)) return nullptr;
  std::string filename = py::robj(arg1).to_string();

  DataTable* dt = DataTable::open_jay(filename);
  py::Frame* frame = py::Frame::from_datatable(dt);
  return frame;
}




//==============================================================================
// PyDatatable getters/setters
//==============================================================================

PyObject* get_isview(obj* self) {
  DataTable* dt = self->ref;
  for (size_t i = 0; i < dt->ncols; ++i) {
    if (dt->columns[i]->rowindex())
      return incref(Py_True);
  }
  return incref(Py_False);
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
 * Return size of the referenced DataTable, and the current `pydatatable::obj`.
 */
PyObject* get_alloc_size(obj* self) {
  DataTable* dt = self->ref;
  size_t sz = dt->memory_footprint();
  sz += sizeof(*self);
  // if (self->ltypes) sz += _PySys_GetSizeOf(self->ltypes);
  // if (self->stypes) sz += _PySys_GetSizeOf(self->stypes);
  // if (self->names) {
  //   PyObject* names = self->names;
  //   sz += _PySys_GetSizeOf(names);
  //   for (Py_ssize_t i = 0; i < Py_SIZE(names); ++i) {
  //     sz += _PySys_GetSizeOf(PyTuple_GET_ITEM(names, i));
  //   }
  // }
  return PyLong_FromSize_t(sz);
}



//==============================================================================
// PyDatatable methods
//==============================================================================

void _clear_types(obj* self) {
  if (self->_frame) self->_frame->_clear_types();
}


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
    size_t i = col->rowindex().nth(0);
    auto f = py_stype_formatters[static_cast<int>(col->stype())];
    return f(col, i);
  } else {
    throw ValueError() << ".scalar() method cannot be applied to a Frame with "
      "shape `[" << dt->nrows << " x " << dt->ncols << "]`";
  }
}


PyObject* check(obj* self, PyObject*) {
  DataTable* dt = self->ref;
  dt->verify_integrity();

  if (self->_frame && self->_frame->stypes) {
    PyObject* stypes = self->_frame->stypes;
    if (!PyTuple_Check(stypes)) {
      throw AssertionError() << "Frame.stypes is not a tuple";
    }
    if (static_cast<size_t>(PyTuple_Size(stypes)) != dt->ncols) {
      throw AssertionError() << "len(Frame.stypes) is " << PyTuple_Size(stypes)
          << ", whereas .ncols = " << dt->ncols;
    }
    for (size_t i = 0; i < dt->ncols; ++i) {
      SType st = dt->columns[i]->stype();
      PyObject* elem = PyTuple_GET_ITEM(stypes, i);
      PyObject* eexp = info(st).py_stype().release();
      if (elem != eexp) {
        throw AssertionError() << "Element " << i << " of Frame.stypes is "
            << elem << ", but the column's type is " << eexp;
      }
    }
  }
  if (self->_frame && self->_frame->ltypes) {
    PyObject* ltypes = self->_frame->ltypes;
    if (!PyTuple_Check(ltypes)) {
      throw AssertionError() << "Frame.ltypes is not a tuple";
    }
    if (static_cast<size_t>(PyTuple_Size(ltypes)) != dt->ncols) {
      throw AssertionError() << "len(Frame.ltypes) is " << PyTuple_Size(ltypes)
          << ", whereas .ncols = " << dt->ncols;
    }
    for (size_t i = 0; i < dt->ncols; ++i) {
      SType st = dt->columns[i]->stype();
      PyObject* elem = PyTuple_GET_ITEM(ltypes, i);
      PyObject* eexp = info(st).py_ltype().release();
      if (elem != eexp) {
        throw AssertionError() << "Element " << i << " of Frame.ltypes is "
            << elem << ", for a column of type " << eexp;
      }
    }
  }

  Py_RETURN_NONE;
}


PyObject* column(obj* self, PyObject* args) {
  DataTable* dt = self->ref;
  int64_t colidx;
  int64_t ncols = static_cast<int64_t>(dt->ncols);
  if (!PyArg_ParseTuple(args, "l:column", &colidx))
    return nullptr;
  if (colidx < -ncols || colidx >= ncols) {
    PyErr_Format(PyExc_ValueError, "Invalid column index %lld", colidx);
    return nullptr;
  }
  if (colidx < 0) colidx += dt->ncols;
  pycolumn::obj* pycol =
      pycolumn::from_column(dt->columns[static_cast<size_t>(colidx)], self, colidx);
  return pycol;
}



PyObject* delete_columns(obj* self, PyObject* args) {
  DataTable* dt = self->ref;
  PyObject* arg1;
  if (!PyArg_ParseTuple(args, "O:delete_columns", &arg1))
    return nullptr;
  py::olist list = py::robj(arg1).to_pylist();
  size_t ncols = list.size();

  std::vector<size_t> cols_to_remove;
  cols_to_remove.reserve(ncols);
  for (size_t i = 0; i < ncols; ++i) {
    cols_to_remove.push_back(list[i].to_size_t());
  }
  dt->delete_columns(cols_to_remove);

  _clear_types(self);
  Py_RETURN_NONE;
}




PyObject* replace_rowindex(obj* self, PyObject* args) {
  DataTable* dt = self->ref;
  PyObject* arg1;
  if (!PyArg_ParseTuple(args, "O:replace_rowindex", &arg1))
    return nullptr;
  RowIndex newri = py::robj(arg1).to_rowindex();

  dt->replace_rowindex(newri);
  Py_RETURN_NONE;
}



PyObject* replace_column_slice(obj* self, PyObject* args) {
  DataTable* dt = self->ref;
  int64_t start;
  size_t count;
  int64_t step;
  PyObject *arg4, *arg5;
  if (!PyArg_ParseTuple(args, "lllOO:replace_column_slice",
                        &start, &count, &step, &arg4, &arg5)) return nullptr;
  RowIndex rows_ri = py::robj(arg4).to_rowindex();
  DataTable* repl = py::robj(arg5).to_frame();
  size_t rrows = repl->nrows;
  size_t rcols = repl->ncols;
  size_t rrows2 = rows_ri? rows_ri.length() : dt->nrows;

  if (!check_slice_triple(start, static_cast<int64_t>(count), step,
                          static_cast<int64_t>(dt->ncols - 1))) {
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

  for (size_t i = 0; i < count; ++i) {
    int64_t j = start + static_cast<int64_t>(i) * step;
    size_t zj = static_cast<size_t>(j);
    Column* replcol = repl->columns[i % rcols];
    if (rows_ri) {
      dt->columns[zj]->replace_values(rows_ri, replcol);
    } else {
      delete dt->columns[zj];
      dt->columns[zj] = replcol->shallowcopy();
    }
  }
  _clear_types(self);
  Py_RETURN_NONE;
}


PyObject* replace_column_array(obj* self, PyObject* args) {
  DataTable* dt = self->ref;
  PyObject *arg1, *arg2, *arg3;
  if (!PyArg_ParseTuple(args, "OOO:replace_column_array", &arg1, &arg2, &arg3))
      return nullptr;
  py::olist cols = py::robj(arg1).to_pylist();
  RowIndex rows_ri = py::robj(arg2).to_rowindex();
  DataTable* repl = py::robj(arg3).to_frame();
  size_t rrows = repl->nrows;
  size_t rcols = repl->ncols;
  size_t rrows2 = rows_ri? rows_ri.length() : dt->nrows;

  bool ok = (rrows == rrows2 || rrows == 1) &&
            (rcols == cols.size() || rcols == 1);
  if (!ok) {
    throw ValueError() << "Invalid replacement Frame: expected [" <<
      rrows2 << " x " << cols.size() << "], but received [" << rrows <<
      " x " << rcols << "]";
  }

  dt->reify();
  repl->reify();

  size_t num_new_cols = 0;
  for (size_t i = 0; i < cols.size(); ++i) {
    py::robj item = cols[i];
    int64_t j = item.to_int64_strict();
    num_new_cols += (j == -1);
    if (j < -1 || j >= static_cast<int64_t>(dt->ncols)) {
      throw ValueError() << "Invalid index for a replacement column: " << j;
    }
  }
  if (num_new_cols) {
    if (rows_ri) {
      throw ValueError() << "Cannot assign to column(s) that are outside of "
                            "the Frame: " << rows_ri;
    }
    size_t newsize = dt->ncols + num_new_cols;
    dt->columns.resize(newsize);
  }
  for (size_t i = 0; i < cols.size(); ++i) {
    py::robj item = cols[i];
    int64_t j = item.to_int64_strict();
    size_t zj = static_cast<size_t>(j);
    Column* replcol = repl->columns[i % rcols];
    if (rows_ri) {
      dt->columns[zj]->replace_values(rows_ri, replcol);
    } else {
      if (j == -1) {
        zj = dt->ncols++;
      } else {
        delete dt->columns[zj];
      }
      dt->columns[zj] = replcol->shallowcopy();
    }
  }

  // Clear cached stypes/ltypes; No need to update names
  _clear_types(self);
  Py_RETURN_NONE;
}


PyObject* rbind(obj* self, PyObject* args) {
  DataTable* dt = self->ref;
  size_t final_ncols;
  PyObject* list;
  if (!PyArg_ParseTuple(args, "lO!:delete_columns",
                        &final_ncols, &PyList_Type, &list))
    return nullptr;
  size_t ndts = static_cast<size_t>(PyList_Size(list));

  constexpr size_t INVALID_INDEX = size_t(-1);
  std::vector<DataTable*> dts;
  std::vector<std::vector<size_t>> cols_to_append(final_ncols);
  for (size_t j = 0; j < final_ncols; ++j) {
    cols_to_append[j].resize(ndts);
  }

  for (size_t i = 0; i < ndts; i++) {
    PyObject* item = PyList_GET_ITEM(list, i);
    DataTable* dti;
    PyObject* colslist;
    if (!PyArg_ParseTuple(item, "O&O",
                          &unwrap, &dti, &colslist))
      return nullptr;
    size_t j = 0;
    if (colslist == Py_None) {
      size_t ncolsi = dti->ncols;
      for (; j < ncolsi; ++j) {
        cols_to_append[j][i] = j;
      }
    } else {
      size_t ncolsi = static_cast<size_t>(PyList_Size(colslist));
      for (; j < ncolsi; ++j) {
        PyObject* itemj = PyList_GET_ITEM(colslist, j);
        cols_to_append[j][i] = (itemj == Py_None)? INVALID_INDEX
                               : PyLong_AsSize_t(itemj);
      }
    }
    for (; j < final_ncols; ++j) {
      cols_to_append[j][i] = INVALID_INDEX;
    }
    dts.push_back(dti);
  }

  dt->rbind(dts, cols_to_append);

  // Clear cached stypes/ltypes
  _clear_types(self);

  Py_RETURN_NONE;
}



PyObject* sort(obj* self, PyObject* args) {
  DataTable* dt = self->ref;
  py::olist arglist = py::robj(args).to_pylist();
  size_t nargs = arglist.size();
  bool last_arg_bool = nargs > 1 && arglist[nargs - 1].is_bool();
  bool make_groups = last_arg_bool? arglist[nargs - 1].to_bool_strict() : false;

  std::vector<size_t> cols;
  for (size_t i = 0; i < nargs - last_arg_bool; ++i) {
    cols.push_back(arglist[i].to_size_t());
  }
  Groupby grpby;
  RowIndex ri = dt->sortby(cols, make_groups? &grpby : nullptr);
  // return pyrowindex::wrap(ri);
  return Py_BuildValue("NN", pyrowindex::wrap(ri),
                             make_groups? pygroupby::wrap(grpby) : none());
}



PyObject* join(obj* self, PyObject* args) {
  PyObject *arg1, *arg2, *arg3;
  if (!PyArg_ParseTuple(args, "OOO:join", &arg1, &arg2, &arg3)) return nullptr;

  DataTable* dt = self->ref;
  DataTable* jdt = py::robj(arg2).to_frame();
  RowIndex ri = py::robj(arg1).to_rowindex();
  py::olist cols_arg = py::robj(arg3).to_pylist();

  if (cols_arg.size() != 1) {
    throw NotImplError() << "Only single-column joins are currently supported";
  }
  size_t i = cols_arg[0].to_size_t();
  if (i >= dt->ncols) {
    throw ValueError() << "Invalid index " << i << " for a Frame with "
        << dt->ncols << " columns";
  }

  Column* col = dt->columns[i]->shallowcopy();
  if (ri) col->replace_rowindex(ri);
  RowIndex join_ri = col->join(jdt->columns[0]);
  delete col;

  return pyrowindex::wrap(join_ri);
}



PyObject* get_min    (obj* self, PyObject*) { return py::Frame::from_datatable(self->ref->min_datatable()); }
PyObject* get_max    (obj* self, PyObject*) { return py::Frame::from_datatable(self->ref->max_datatable()); }
PyObject* get_mode   (obj* self, PyObject*) { return py::Frame::from_datatable(self->ref->mode_datatable()); }
PyObject* get_mean   (obj* self, PyObject*) { return py::Frame::from_datatable(self->ref->mean_datatable()); }
PyObject* get_sd     (obj* self, PyObject*) { return py::Frame::from_datatable(self->ref->sd_datatable()); }
PyObject* get_skew   (obj* self, PyObject*) { return py::Frame::from_datatable(self->ref->skew_datatable()); }
PyObject* get_kurt   (obj* self, PyObject*) { return py::Frame::from_datatable(self->ref->kurt_datatable()); }
PyObject* get_sum    (obj* self, PyObject*) { return py::Frame::from_datatable(self->ref->sum_datatable()); }
PyObject* get_countna(obj* self, PyObject*) { return py::Frame::from_datatable(self->ref->countna_datatable()); }
PyObject* get_nunique(obj* self, PyObject*) { return py::Frame::from_datatable(self->ref->nunique_datatable()); }
PyObject* get_nmodal (obj* self, PyObject*) { return py::Frame::from_datatable(self->ref->nmodal_datatable()); }

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

  for (size_t i = 0; i < dt->ncols; ++i) {
    Column* oldcol = dt->columns[i];
    if (!oldcol->rowindex()) continue;
    Column* newcol = oldcol->shallowcopy();
    newcol->reify();
    delete oldcol;
    dt->columns[i] = newcol;
  }
  dt->rowindex.clear();

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
  PyObject* arg1, *arg2, *arg3;
  if (!PyArg_ParseTuple(args, "OOO:save_jay", &arg1, &arg2, &arg3))
    return nullptr;

  auto filename = py::robj(arg1).to_string();
  auto colnames = py::robj(arg2).to_stringlist();
  auto strategy = py::robj(arg3).to_string();
  auto sstrategy = (strategy == "mmap")  ? WritableBuffer::Strategy::Mmap :
                   (strategy == "write") ? WritableBuffer::Strategy::Write :
                                           WritableBuffer::Strategy::Auto;

  if (colnames.size() != dt->ncols) {
    throw ValueError()
      << "The list of column names has wrong length: " << colnames.size();
  }

  dt->save_jay(filename, colnames, sstrategy);
  Py_RETURN_NONE;
}



//------------------------------------------------------------------------------
// Misc
//------------------------------------------------------------------------------



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
  METHODv(replace_rowindex),
  METHODv(replace_column_slice),
  METHODv(replace_column_array),
  METHODv(rbind),
  METHODv(sort),
  METHODv(join),
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
  METHODv(use_stype_for_buffers),
  METHODv(save_jay),
  {nullptr, nullptr, 0, nullptr}           /* sentinel */
};

static PyGetSetDef datatable_getseters[] = {
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
  nullptr,                            /* tp_as_sync */
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
