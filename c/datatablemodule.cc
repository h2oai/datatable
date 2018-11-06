//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#define dt_DATATABLEMODULE_cc
#include "datatablemodule.h"
#include <Python.h>
#include "capi.h"
#include "csv/py_csv.h"
#include "csv/writer.h"
#include "expr/py_expr.h"
#include "extras/aggregator.h"
#include "extras/ftrl.h"
#include "frame/py_frame.h"
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
#include "ztest.h"

extern void init_jay();

namespace py {
  PyObject* fread_fn = nullptr;
  PyObject* fallback_makedatatable = nullptr;
}


PyMODINIT_FUNC PyInit__datatable(void);


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
  else if (n == 6) replace_dtWarning(fnref);
  else if (n == 7) py::Frame_Type = fnref;
  else if (n == 8) py::fread_fn = fnref;
  else if (n == 9) py::fallback_makedatatable = fnref;
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

void DatatableModule::init_methods() {
  add(METHODv(pycolumnset::columns_from_mixed));
  add(METHODv(pycolumnset::columns_from_slice));
  add(METHODv(pycolumnset::columns_from_columns));
  add(METHODv(pycolumn::column_from_list));
  add(METHODv(pyrowindex::rowindex_from_slice));
  add(METHODv(pyrowindex::rowindex_from_slicelist));
  add(METHODv(pyrowindex::rowindex_from_array));
  add(METHODv(pyrowindex::rowindex_from_column));
  add(METHODv(pyrowindex::rowindex_from_filterfn));
  add(METHODv(pydatatable::datatable_load));
  add(METHODv(pydatatable::open_jay));
  add(METHODv(pydatatable::install_buffer_hooks));
  add(METHODv(gread));
  add(METHODv(write_csv));
  add(METHODv(exec_function));
  add(METHODv(register_function));
  add(METHOD0(get_internal_function_ptrs));
  add(METHOD0(get_integer_sizes));
  add(METHODv(expr_binaryop));
  add(METHODv(expr_cast));
  add(METHODv(expr_column));
  add(METHODv(expr_reduceop));
  add(METHODv(expr_count));
  add(METHODv(expr_unaryop));
  add(METHOD0(is_debug_mode));
  add(METHOD0(has_omp_support));
  add(METHODv(aggregate));
  add(METHODv(ftrl));
  init_methods_str();
  init_methods_options();
  init_methods_sets();
}


/* Called when Python program imports the module */
PyMODINIT_FUNC
PyInit__datatable()
{
  init_csvwrite_constants();
  init_exceptions();

  static DatatableModule dtmod;
  PyObject* m = dtmod.init();

  // Initialize submodules
  if (!init_py_types(m)) return nullptr;
  if (!pydatawindow::static_init(m)) return nullptr;
  if (!pycolumn::static_init(m)) return nullptr;
  if (!pycolumnset::static_init(m)) return nullptr;
  if (!pydatatable::static_init(m)) return nullptr;
  if (!pygroupby::static_init(m)) return nullptr;
  if (!pyrowindex::static_init(m)) return nullptr;
  if (!init_py_encodings(m)) return nullptr;
  init_jay();

  try {
    py::Frame::Type::init(m);

    #ifdef DTTEST
      dttest::run_tests();
    #endif

  } catch (const std::exception& e) {
    exception_to_python(e);
    return nullptr;
  }

  return m;
}
