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
#include "utils/pyobj.h"


/**
 * Create a new RowIndex_PyObject by wrapping the provided RowIndex.
 * The returned py-object will hold a shallow copy of `src`.
 */
PyObject* pyrowindex(const RowIndex& rowindex) {
  PyObject* res = PyObject_CallObject((PyObject*) &RowIndex_PyType, NULL);
  static_cast<RowIndex_PyObject*>(res)->ref = new RowIndex(rowindex);
  return res;
}
#define py pyrowindex



//==============================================================================
// Constructors
//==============================================================================

PyObject* rowindex_from_slice(PyObject*, PyObject* args) {
  int64_t start, count, step;
  if (!PyArg_ParseTuple(args, "LLL:rowindex_from_slice",
                        &start, &count, &step)) return nullptr;
  return py(RowIndex::from_slice(start, count, step));
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
  dt::array<int64_t> starts(n);
  dt::array<int64_t> counts(n);
  dt::array<int64_t> steps(n);

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
  return py(RowIndex::from_slices(starts, counts, steps));
}


PyObject* rowindex_from_array(PyObject*, PyObject* args) {
  dt::array<int32_t> data32;
  dt::array<int64_t> data64;
  PyObject* list;
  if (!PyArg_ParseTuple(args, "O!:rowindex_from_array",
                        &PyList_Type, &list)) return NULL;

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
  return data32? py(RowIndex::from_array32(std::move(data32)))
               : py(RowIndex::from_array64(std::move(data64)));
}


PyObject* rowindex_from_column(PyObject*, PyObject* args) {
  Column* col;
  if (!PyArg_ParseTuple(args, "O&:rowindex_from_column",
                        &pycolumn::unwrap, &col)) return nullptr;
  return py(RowIndex::from_column(col));
}


PyObject* rowindex_from_filterfn(PyObject*, PyObject* args)
{
  long long _fnptr;
  long long _nrows;
  if (!PyArg_ParseTuple(args, "LL:rowindex_from_filterfn",
                        &_fnptr, &_nrows))
      return nullptr;

  int64_t nrows = (int64_t) _nrows;
  if (nrows <= INT32_MAX) {
    filterfn32 *fnptr = (filterfn32*)_fnptr;
    return py(RowIndex::from_filterfn32(fnptr, nrows, 0));
  } else {
    filterfn64 *fnptr = (filterfn64*)_fnptr;
    return py(RowIndex::from_filterfn64(fnptr, nrows, 0));
  }
}



PyObject* rowindex_uplift(PyObject*, PyObject* args) {
  PyObject *arg1, *arg2;
  if (!PyArg_ParseTuple(args, "OO:rowindex_uplift", &arg1, &arg2))
    return nullptr;
  RowIndex ri = PyObj(arg1).as_rowindex();
  DataTable* dt = PyObj(arg2).as_datatable();

  return py(dt->rowindex.merged_with(ri));
}



//==============================================================================
//  RowIndex PyObject
//==============================================================================

static void dealloc(RowIndex_PyObject *self)
{
  delete self->ref;
  self->ref = nullptr;
  Py_TYPE(self)->tp_free((PyObject*)self);
}


static PyObject* repr(RowIndex_PyObject *self)
{
  CATCH_EXCEPTIONS(
    RowIndex& rz = *(self->ref);
    if (rz.isabsent())
      return PyUnicode_FromString("_RowIndex(NULL)");
    if (rz.isarr32()) {
      return PyUnicode_FromFormat("_RowIndex(int32[%ld])", rz.length());
    }
    if (rz.isarr64()) {
      return PyUnicode_FromFormat("_RowIndex(int64[%ld])", rz.length());
    }
    if (rz.isslice()) {
      return PyUnicode_FromFormat("_RowIndex(%ld:%ld:%ld)",
          rz.slice_start(), rz.length(), rz.slice_step());
    }
    return nullptr;
  );
}


static PyObject* tolist(RowIndex_PyObject *self, PyObject* args)
{
  if (!PyArg_ParseTuple(args, "")) return NULL;
  RowIndex& rz = *(self->ref);

  CATCH_EXCEPTIONS(
    PyObject *list = PyList_New((Py_ssize_t) rz.length());
    if (rz.isarr32()) {
      int32_t n = static_cast<int32_t>(rz.length());
      const int32_t* a = rz.indices32();
      for (int32_t i = 0; i < n; ++i) {
        PyList_SET_ITEM(list, i, PyLong_FromLong(a[i]));
      }
    }
    if (rz.isarr64()) {
      int64_t n = rz.length();
      const int64_t* a = rz.indices64();
      for (int64_t i = 0; i < n; ++i) {
        PyList_SET_ITEM(list, i, PyLong_FromLong(a[i]));
      }
    }
    if (rz.isslice()) {
      int64_t n = rz.length();
      int64_t start = rz.slice_start();
      int64_t step = rz.slice_step();
      for (int64_t i = 0; i < n; ++i) {
        PyList_SET_ITEM(list, i, PyLong_FromLong(start + i*step));
      }
    }
    return list;
  );
}


static PyObject *getptr(RowIndex_PyObject *self, PyObject*) {
  CATCH_EXCEPTIONS(
    RowIndex* ri = self->ref;
    return PyLong_FromSize_t(reinterpret_cast<size_t>(ri));
  );
}



//==============================================================================
// DataTable type definition
//==============================================================================

#define METHOD0_(name) {#name, (PyCFunction)name, METH_VARARGS, NULL}
#define METHOD1_(name) {#name, (PyCFunction)name, METH_NOARGS, NULL}
static PyMethodDef rowindex_methods[] = {
    METHOD0_(tolist),
    METHOD1_(getptr),
    {NULL, NULL, 0, NULL}           /* sentinel */
};


PyTypeObject RowIndex_PyType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "_datatable.RowIndex",              /* tp_name */
    sizeof(RowIndex_PyObject),          /* tp_basicsize */
    0,                                  /* tp_itemsize */
    (destructor)dealloc,                /* tp_dealloc */
    0,                                  /* tp_print */
    0,                                  /* tp_getattr */
    0,                                  /* tp_setattr */
    0,                                  /* tp_compare */
    (reprfunc)repr,                     /* tp_repr */
    0,                                  /* tp_as_number */
    0,                                  /* tp_as_sequence */
    0,                                  /* tp_as_mapping */
    0,                                  /* tp_hash  */
    0,                                  /* tp_call */
    0,                                  /* tp_str */
    0,                                  /* tp_getattro */
    0,                                  /* tp_setattro */
    0,                                  /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT,                 /* tp_flags */
    0,                                  /* tp_doc */
    0,                                  /* tp_traverse */
    0,                                  /* tp_clear */
    0,                                  /* tp_richcompare */
    0,                                  /* tp_weaklistoffset */
    0,                                  /* tp_iter */
    0,                                  /* tp_iternext */
    rowindex_methods,                   /* tp_methods */
    0,                                  /* tp_members */
    0,                                  /* tp_getset */
    0,                                  /* tp_base */
    0,                                  /* tp_dict */
    0,                                  /* tp_descr_get */
    0,                                  /* tp_descr_set */
    0,                                  /* tp_dictoffset */
    0,                                  /* tp_init */
    0,0,0,0,0,0,0,0,0,0,0,0
};


// Add PyRowIndex object to the Python module
int init_py_rowindex(PyObject *module)
{
    RowIndex_PyType.tp_new = PyType_GenericNew;
    if (PyType_Ready(&RowIndex_PyType) < 0) return 0;
    Py_INCREF(&RowIndex_PyType);
    PyModule_AddObject(module, "RowIndex", (PyObject*) &RowIndex_PyType);
    return 1;
}
