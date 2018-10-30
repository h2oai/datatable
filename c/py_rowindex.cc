//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#define dt_PY_ROWINDEX_cc
#include "py_rowindex.h"
#include "py_datatable.h"
#include "py_column.h"
#include "py_utils.h"
#include "python/obj.h"

namespace pyrowindex
{


/**
 * Create a new pyRowIndex object by wrapping the provided RowIndex.
 * The returned py-object will hold a shallow copy of source rowindex.
 */
PyObject* wrap(const RowIndex& rowindex) {
  PyObject* pytype = reinterpret_cast<PyObject*>(&pyrowindex::type);
  PyObject* pyri = PyObject_CallObject(pytype, nullptr);
  if (pyri) {
    auto pypyri = static_cast<pyrowindex::obj*>(pyri);
    pypyri->ref = new RowIndex(rowindex);
  }
  return pyri;
}



//==============================================================================
// Generic Python API
//==============================================================================

PyObject* rowindex_from_slice(PyObject*, PyObject* args) {
  int64_t start, count, step;
  if (!PyArg_ParseTuple(args, "LLL:rowindex_from_slice",
                        &start, &count, &step)) return nullptr;
  return wrap(RowIndex::from_slice(start, count, step));
}


PyObject* rowindex_from_slicelist(PyObject*, PyObject* args) {
  PyObject* pystarts;
  PyObject* pycounts;
  PyObject* pysteps;
  if (!PyArg_ParseTuple(args, "O!O!O!:rowindex_from_slicelist",
                        &PyList_Type, &pystarts,
                        &PyList_Type, &pycounts,
                        &PyList_Type, &pysteps)) return nullptr;

  int64_t n1 = PyList_Size(pystarts);
  int64_t n2 = PyList_Size(pycounts);
  int64_t n3 = PyList_Size(pysteps);
  if (n1 < n2 || n1 < n3) {
    throw ValueError() << "`starts` array cannot be shorter than `counts` or "
                          "`steps` arrays";
  }
  size_t n = static_cast<size_t>(n1);
  arr64_t starts(n);
  arr64_t counts(n);
  arr64_t steps(n);

  // Convert Pythonic lists into regular C arrays of longs
  for (int64_t i = 0; i < n1; ++i) {
    int64_t start = PyLong_AsSsize_t(PyList_GET_ITEM(pystarts, i));
    int64_t count = i < n2? PyLong_AsSsize_t(PyList_GET_ITEM(pycounts, i)) : 1;
    int64_t step  = i < n3? PyLong_AsSsize_t(PyList_GET_ITEM(pysteps, i)) : 1;
    if ((start == -1 || count  == -1 || step == -1) &&
        PyErr_Occurred()) return nullptr;
    size_t ii = static_cast<size_t>(i);
    starts[ii] = start;
    counts[ii] = count;
    steps[ii] = step;
  }
  return wrap(RowIndex::from_slices(starts, counts, steps));
}


PyObject* rowindex_from_array(PyObject*, PyObject* args) {
  arr32_t data32;
  arr64_t data64;
  PyObject* list;
  if (!PyArg_ParseTuple(args, "O!:rowindex_from_array",
                        &PyList_Type, &list)) return nullptr;

  // Convert Pythonic List into a regular C array of int32's/int64's
  int64_t len = PyList_Size(list);
  size_t zlen = static_cast<size_t>(len);
  if (len <= INT32_MAX) {
    data32.resize(zlen);
  } else {
    data64.resize(zlen);
  }
  for (size_t i = 0; i < zlen; ++i) {
    int64_t x = PyLong_AsSsize_t(PyList_GET_ITEM(list, i));
    if (x == -1 && PyErr_Occurred()) return nullptr;
    if (x < 0) {
      throw ValueError() << "Negative indices not allowed: " << x;
    }
    if (data64) {
      data64[i] = x;
    } else if (x <= INT32_MAX) {
      data32[i] = static_cast<int32_t>(x);
    } else {
      data64.resize(zlen);
      for (size_t j = 0; j < i; ++j) {
        data64[j] = static_cast<int64_t>(data32[j]);
      }
      data32.resize(0);
      data64[i] = x;
    }
  }
  // Construct and return the RowIndex object
  return data32? wrap(RowIndex::from_array32(std::move(data32)))
               : wrap(RowIndex::from_array64(std::move(data64)));
}


PyObject* rowindex_from_column(PyObject*, PyObject* args) {
  Column* col;
  if (!PyArg_ParseTuple(args, "O&:rowindex_from_column",
                        &pycolumn::unwrap, &col)) return nullptr;
  return wrap(RowIndex::from_column(col));
}


PyObject* rowindex_from_filterfn(PyObject*, PyObject* args)
{
  long long _fnptr;
  long long _nrows;
  if (!PyArg_ParseTuple(args, "LL:rowindex_from_filterfn",
                        &_fnptr, &_nrows))
      return nullptr;

  int64_t nrows = static_cast<int64_t>(_nrows);
  if (nrows <= INT32_MAX) {
    filterfn32* fnptr = reinterpret_cast<filterfn32*>(_fnptr);
    return wrap(RowIndex::from_filterfn32(fnptr, nrows, 0));
  } else {
    filterfn64* fnptr = reinterpret_cast<filterfn64*>(_fnptr);
    return wrap(RowIndex::from_filterfn64(fnptr, nrows, 0));
  }
}



//==============================================================================
// Getters/setters
//==============================================================================

PyObject* get_nrows(obj* self) {
  return PyLong_FromSize_t(self->ref->length());
}

PyObject* get_min(obj* self) {
  return PyLong_FromLongLong(self->ref->min());
}

PyObject* get_max(obj* self) {
  return PyLong_FromLongLong(self->ref->max());
}

PyObject* get_ptr(obj* self) {
  const void* ptr = self->ref->ptr();
  return PyLong_FromSize_t(reinterpret_cast<size_t>(ptr));
}




//==============================================================================
// Methods
//==============================================================================

static void dealloc(obj* self) {
  delete self->ref;
  self->ref = nullptr;
  Py_TYPE(self)->tp_free(self);
}


static PyObject* repr(obj* self)
{
  RowIndex& rz = *(self->ref);
  if (rz.isabsent())
    return PyUnicode_FromString("_RowIndex(nullptr)");
  if (rz.isarr32()) {
    return PyUnicode_FromFormat("_RowIndex(int32[%ld])", rz.length());
  }
  if (rz.isarr64()) {
    return PyUnicode_FromFormat("_RowIndex(int64[%ld])", rz.length());
  }
  if (rz.isslice()) {
    return PyUnicode_FromFormat("_RowIndex(%ld/%ld/%ld)",
        rz.slice_start(), rz.length(), rz.slice_step());
  }
  return nullptr;
}


PyObject* tolist(obj* self, PyObject*)
{
  RowIndex& ri = *(self->ref);
  int64_t n = static_cast<int64_t>(ri.length());

  PyObject* list = PyList_New(n);
  if (!list) return nullptr;
  if (ri.isarr32()) {
    int32_t n32 = static_cast<int32_t>(n);
    const int32_t* a = ri.indices32();
    for (int32_t i = 0; i < n32; ++i) {
      PyList_SET_ITEM(list, i, PyLong_FromLong(a[i]));
    }
  }
  if (ri.isarr64()) {
    const int64_t* a = ri.indices64();
    for (int64_t i = 0; i < n; ++i) {
      PyList_SET_ITEM(list, i, PyLong_FromLong(a[i]));
    }
  }
  if (ri.isslice()) {
    int64_t start = ri.slice_start();
    int64_t step = ri.slice_step();
    for (int64_t i = 0; i < n; ++i) {
      PyList_SET_ITEM(list, i, PyLong_FromLong(start + i*step));
    }
  }
  return list;
}


PyObject* uplift(obj* self, PyObject* args) {
  PyObject* arg1;
  if (!PyArg_ParseTuple(args, "O:RowIndex.uplift", &arg1)) return nullptr;
  RowIndex& r1 = *(self->ref);
  RowIndex  r2 = py::obj(arg1).to_rowindex();
  RowIndex res = r1.uplift(r2);
  return wrap(res);
}


PyObject* inverse(obj* self, PyObject* args) {
  RowIndex& ri = *(self->ref);
  size_t nrows;
  if (!PyArg_ParseTuple(args, "n:RowIndex.inverse", &nrows))
    return nullptr;
  RowIndex res = ri.inverse(nrows);
  return wrap(res);
}



//==============================================================================
// DataTable type definition
//==============================================================================

static PyGetSetDef rowindex_getsetters[] = {
  GETTER(nrows),
  GETTER(min),
  GETTER(max),
  GETTER(ptr),
  {nullptr, nullptr, nullptr, nullptr, nullptr}  /* sentinel */
};

static PyMethodDef rowindex_methods[] = {
  METHOD0(tolist),
  METHODv(uplift),
  METHODv(inverse),
  {nullptr, nullptr, 0, nullptr}           /* sentinel */
};


PyTypeObject type = {
  PyVarObject_HEAD_INIT(nullptr, 0)
  "datatable.core.RowIndex",          /* tp_name */
  sizeof(obj),                        /* tp_basicsize */
  0,                                  /* tp_itemsize */
  DESTRUCTOR,                         /* tp_dealloc */
  nullptr,                            /* tp_print */
  nullptr,                            /* tp_getattr */
  nullptr,                            /* tp_setattr */
  nullptr,                            /* tp_compare */
  REPR,                               /* tp_repr */
  nullptr,                            /* tp_as_number */
  nullptr,                            /* tp_as_sequence */
  nullptr,                            /* tp_as_mapping */
  nullptr,                            /* tp_hash  */
  nullptr,                            /* tp_call */
  nullptr,                            /* tp_str */
  nullptr,                            /* tp_getattro */
  nullptr,                            /* tp_setattro */
  nullptr,                            /* tp_as_buffer */
  Py_TPFLAGS_DEFAULT,                 /* tp_flags */
  nullptr,                            /* tp_doc */
  nullptr,                            /* tp_traverse */
  nullptr,                            /* tp_clear */
  nullptr,                            /* tp_richcompare */
  0,                                  /* tp_weaklistoffset */
  nullptr,                            /* tp_iter */
  nullptr,                            /* tp_iternext */
  rowindex_methods,                   /* tp_methods */
  nullptr,                            /* tp_members */
  rowindex_getsetters,                /* tp_getset */
  nullptr,                            /* tp_base */
  nullptr,                            /* tp_dict */
  nullptr,                            /* tp_descr_get */
  nullptr,                            /* tp_descr_set */
  0,                                  /* tp_dictoffset */
  nullptr,                            /* tp_init */
  nullptr,
  nullptr,
  nullptr,
  nullptr,
  nullptr,
  nullptr,
  nullptr,
  nullptr,
  nullptr,
  nullptr,
  0,
  nullptr,
};


// Add PyRowIndex object to the Python module
int static_init(PyObject *module) {
  type.tp_new = PyType_GenericNew;
  if (PyType_Ready(&type) < 0) return 0;
  PyObject* typeobj = reinterpret_cast<PyObject*>(&type);
  Py_INCREF(typeobj);
  PyModule_AddObject(module, "RowIndex", typeobj);
  return 1;
}


};  // namespace pyrowindex
