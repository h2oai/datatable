//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#define dt_DATATABLEMODULE_cc
#include <Python.h>
#include "capi.h"
#include "csv/py_csv.h"
#include "csv/writer.h"
#include "expr/py_expr.h"
#include "options.h"
#include "py_column.h"
#include "py_columnset.h"
#include "py_datatable.h"
#include "py_datawindow.h"
#include "py_encodings.h"
#include "py_groupby.h"
#include "py_rowindex.h"
#include "py_types.h"
#include "py_utils.h"
#include "utils/assert.h"


PyMODINIT_FUNC PyInit__datatable(void);
extern PyObject* Py_One, *Py_Zero;

PyObject* Py_One;
PyObject* Py_Zero;

#define HOMEFLAG dt_DATATABLEMODULE_cc

DECLARE_FUNCTION(
  get_integer_sizes,
  "get_integer_sizes()\n\n",
  HOMEFLAG)

DECLARE_FUNCTION(
  get_internal_function_ptrs,
  "get_internal_function_ptrs()\n\n",
  HOMEFLAG)

DECLARE_FUNCTION(
  register_function,
  "register_function()\n\n",
  HOMEFLAG)

DECLARE_FUNCTION(
  exec_function,
  "exec_function()\n\n",
  HOMEFLAG)

DECLARE_FUNCTION(
  is_debug_mode,
  "is_debug_mode()\n\n",
  HOMEFLAG)

DECLARE_FUNCTION(
  has_omp_support,
  "has_omp_support()\n\n"
  "Returns True if datatable was built with OMP support, and False otherwise.\n"
  "Without OMP datatable will be significantly slower, performing all\n"
  "operations in single-threaded mode.\n",
  HOMEFLAG)


PyObject* exec_function(PyObject* self, PyObject* args) {
  void* fnptr;
  PyObject* fnargs = nullptr;
  if (!PyArg_ParseTuple(args, "l|O:exec_function", &fnptr, &fnargs))
      return nullptr;

  return reinterpret_cast<PyCFunction>(fnptr)(self, fnargs);
}


PyObject* register_function(PyObject*, PyObject *args) {
  int n = -1;
  PyObject* fnref = nullptr;
  if (!PyArg_ParseTuple(args, "iO:register_function", &n, &fnref))
      return nullptr;

  if (!PyCallable_Check(fnref)) {
    throw TypeError() << "parameter `fn` must be callable";
  }
  Py_XINCREF(fnref);
  if (n == 1) pycolumn::fn_hexview = fnref;
  else if (n == 2) init_py_stype_objs(fnref);
  else if (n == 3) init_py_ltype_objs(fnref);
  else if (n == 4) replace_typeError(fnref);
  else if (n == 5) replace_valueError(fnref);
  else {
    throw ValueError() << "Incorrect function index: " << n;
  }
  return none();
}


#define ADD(f) \
  PyTuple_SetItem(res, i++, PyLong_FromSize_t(reinterpret_cast<size_t>(f)))

PyObject* get_internal_function_ptrs(PyObject*, PyObject*) {
  const int SIZE = 6;
  int i = 0;
  PyObject *res = PyTuple_New(SIZE);
  if (!res) return nullptr;

  ADD(dt::malloc<void>);
  ADD(dt::realloc<void>);
  ADD(dt::free);
  ADD(datatable_get_column_data);
  ADD(datatable_unpack_slicerowindex);
  ADD(datatable_unpack_arrayrowindex);

  xassert(i == SIZE);
  return res;
}


PyObject* get_integer_sizes(PyObject*, PyObject*) {
  const int SIZE = 5;
  int i = 0;
  PyObject *res = PyTuple_New(SIZE);
  if (!res) return nullptr;

  ADD(sizeof(short int));
  ADD(sizeof(int));
  ADD(sizeof(long int));
  ADD(sizeof(long long int));
  ADD(sizeof(size_t));

  xassert(i == SIZE);
  return res;
}
#undef ADD


PyObject* is_debug_mode(PyObject*, PyObject*) {
  #ifdef DTDEBUG
    return incref(Py_True);
  #else
    return incref(Py_False);
  #endif
}

PyObject* has_omp_support(PyObject*, PyObject*) {
  #ifdef DTNOOPENMP
    return incref(Py_False);
  #else
    return incref(Py_True);
  #endif
}



//------------------------------------------------------------------------------
// Module definition
//------------------------------------------------------------------------------

static PyMethodDef DatatableModuleMethods[] = {
    METHODv(pycolumnset::columns_from_mixed),
    METHODv(pycolumnset::columns_from_slice),
    METHODv(pycolumnset::columns_from_array),
    METHODv(pycolumnset::columns_from_columns),
    METHODv(pycolumn::column_from_list),
    METHODv(pyrowindex::rowindex_from_slice),
    METHODv(pyrowindex::rowindex_from_slicelist),
    METHODv(pyrowindex::rowindex_from_array),
    METHODv(pyrowindex::rowindex_from_column),
    METHODv(pyrowindex::rowindex_from_filterfn),
    METHODv(pydatatable::datatable_from_list),
    METHODv(pydatatable::datatable_load),
    METHODv(pydatatable::install_buffer_hooks),
    METHODv(config::set_option),
    METHODv(gread),
    METHODv(write_csv),
    METHODv(exec_function),
    METHODv(register_function),
    METHOD0(get_internal_function_ptrs),
    METHOD0(get_integer_sizes),
    METHODv(expr_binaryop),
    METHODv(expr_cast),
    METHODv(expr_column),
    METHODv(expr_reduceop),
    METHODv(expr_unaryop),
    METHOD0(is_debug_mode),
    METHOD0(has_omp_support),

    {nullptr, nullptr, 0, nullptr}  /* Sentinel */
};

static PyModuleDef datatablemodule = {
    PyModuleDef_HEAD_INIT,
    "_datatable",  /* name of the module */
    "module doc",  /* module documentation */
    -1,            /* size of per-interpreter state of the module, or -1
                      if the module keeps state in global variables */
    DatatableModuleMethods,

    // https://docs.python.org/3/c-api/module.html#multi-phase-initialization
    nullptr,       /* m_slots */
    nullptr,       /* m_traverse */
    nullptr,       /* m_clear */
    nullptr,       /* m_free */
};


/* Called when Python program imports the module */
PyMODINIT_FUNC
PyInit__datatable(void) {
    init_csvwrite_constants();
    init_exceptions();

    Py_One = PyLong_FromLong(1);
    Py_Zero = PyLong_FromLong(0);

    // Instantiate module object
    PyObject* m = PyModule_Create(&datatablemodule);
    if (m == nullptr) return nullptr;

    // Initialize submodules
    if (!init_py_types(m)) return nullptr;
    if (!pydatawindow::static_init(m)) return nullptr;
    if (!pycolumn::static_init(m)) return nullptr;
    if (!pycolumnset::static_init(m)) return nullptr;
    if (!pydatatable::static_init(m)) return nullptr;
    if (!pygroupby::static_init(m)) return nullptr;
    if (!pyrowindex::static_init(m)) return nullptr;
    if (!init_py_encodings(m)) return nullptr;

    return m;
}
