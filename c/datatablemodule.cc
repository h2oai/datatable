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
#include "../datatable/include/datatable.h"
#include "csv/py_csv.h"
#include "csv/writer.h"
#include "expr/base_expr.h"
#include "expr/by_node.h"
#include "expr/join_node.h"
#include "expr/py_expr.h"
#include "expr/sort_node.h"
#include "extras/aggregator.h"
#include "extras/py_ftrl.h"
#include "frame/py_frame.h"
#include "options.h"
#include "py_column.h"
#include "py_datatable.h"
#include "py_encodings.h"
#include "py_rowindex.h"
#include "py_types.h"
#include "py_utils.h"
#include "utils/assert.h"
#include "ztest.h"

extern void init_jay();

namespace py {
  PyObject* fread_fn = nullptr;
}


PyMODINIT_FUNC PyInit__datatable(void);


#define HOMEFLAG dt_DATATABLEMODULE_cc

DECLARE_FUNCTION(
  get_integer_sizes,
  "get_integer_sizes()\n\n",
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
  else {
    throw ValueError() << "Incorrect function index: " << n;
  }
  return none();
}


#define ADD(f) \
  PyTuple_SetItem(res, i++, PyLong_FromSize_t(reinterpret_cast<size_t>(f)))


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
// These functions are exported as `datatable.internal.*`
//------------------------------------------------------------------------------

static std::pair<DataTable*, size_t> _unpack_args(const py::PKArgs& args) {
  if (!args[0] || !args[1]) throw ValueError() << "Expected 2 arguments";
  DataTable* dt = args[0].to_frame();
  size_t col    = args[1].to_size_t();

  if (!dt) throw TypeError() << "First parameter should be a Frame";
  if (col >= dt->ncols) throw ValueError() << "Index out of bounds";
  return std::make_pair(dt, col);
}


static py::PKArgs fn_frame_column_rowindex(
    2, 0, 0, false, false, {"frame", "i"},
    "frame_column_rowindex",
R"(frame_column_rowindex(frame, i)
--

Return the RowIndex of the `i`th column of the `frame`, or None if that column
has no row index.
)",

[](const py::PKArgs& args) -> py::oobj {
  auto u = _unpack_args(args);
  DataTable* dt = u.first;
  size_t col = u.second;

  RowIndex ri = dt->columns[col]->rowindex();
  return ri? py::orowindex(ri) : py::None();
});


static py::PKArgs fn_frame_column_data_r(
    2, 0, 0, false, false, {"frame", "i"},
    "frame_column_data_r",
R"(frame_column_data_r(frame, i)
--

Return C pointer to the main data array of the column `frame[i]`. The pointer
is returned as a `ctypes.c_void_p` object.
)",

[](const py::PKArgs& args) -> py::oobj {
  static py::oobj c_void_p = py::oobj::import("ctypes", "c_void_p");

  auto u = _unpack_args(args);
  DataTable* dt = u.first;
  size_t col = u.second;
  const void* ptr = dt->columns[col]->data();
  py::otuple init_args(1);
  init_args.set(0, py::oint(reinterpret_cast<size_t>(ptr)));
  return c_void_p.call(init_args);
});




//------------------------------------------------------------------------------
// Module definition
//------------------------------------------------------------------------------

void DatatableModule::init_methods() {
  add(METHODv(pycolumn::column_from_list));
  add(METHODv(pydatatable::datatable_load));
  add(METHODv(pydatatable::open_jay));
  add(METHODv(pydatatable::install_buffer_hooks));
  add(METHODv(gread));
  add(METHODv(write_csv));
  add(METHODv(exec_function));
  add(METHODv(register_function));
  add(METHOD0(get_integer_sizes));
  add(METHOD0(is_debug_mode));
  add(METHOD0(has_omp_support));

  ADDFN(fn_frame_column_rowindex);
  ADDFN(fn_frame_column_data_r);

  init_methods_aggregate();
  init_methods_join();
  init_methods_kfold();
  init_methods_options();
  init_methods_repeat();
  init_methods_sets();
  init_methods_str();
  #ifdef DTTEST
    init_tests();
  #endif
}


/* Called when Python program imports the module */
PyMODINIT_FUNC
PyInit__datatable()
{
  init_csvwrite_constants();
  init_exceptions();

  force_stype = SType::VOID;

  static DatatableModule dtmod;
  PyObject* m = dtmod.init();

  try {
    // Initialize submodules
    if (!init_py_types(m)) return nullptr;
    if (!pycolumn::static_init(m)) return nullptr;
    if (!pydatatable::static_init(m)) return nullptr;
    if (!init_py_encodings(m)) return nullptr;
    init_jay();

    py::Frame::Type::init(m);
    py::Ftrl::Type::init(m);
    py::base_expr::Type::init(m);
    py::orowindex::pyobject::Type::init(m);
    py::oby::init(m);
    py::ojoin::init(m);
    py::osort::init(m);

  } catch (const std::exception& e) {
    exception_to_python(e);
    return nullptr;
  }

  return m;
}
