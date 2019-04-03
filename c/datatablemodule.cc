//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#include <exception>       // std::exception
#include <iostream>        // std::cerr
#include <mutex>           // std::mutex, std::lock_guard
#include <thread>          // std::this_thread
#include <sstream>         // std::stringstream
#include <unordered_map>   // std::unordered_map
#include <utility>         // std::pair, std::make_pair, std::move
#include <Python.h>
#include "../datatable/include/datatable.h"
#include "expr/base_expr.h"
#include "expr/by_node.h"
#include "expr/join_node.h"
#include "expr/py_expr.h"
#include "expr/sort_node.h"
#include "models/aggregator.h"
#include "models/py_ftrl.h"
#include "frame/py_frame.h"
#include "python/_all.h"
#include "python/string.h"
#include "parallel/api.h"
#include "datatablemodule.h"
#include "options.h"
#include "py_encodings.h"
#include "py_rowindex.h"
#include "utils/assert.h"
#include "ztest.h"


namespace py {
  PyObject* fread_fn = nullptr;
}



//------------------------------------------------------------------------------
// These functions are exported as `datatable.internal.*`
//------------------------------------------------------------------------------

static std::pair<DataTable*, size_t>
_unpack_frame_column_args(const py::PKArgs& args)
{
  if (!args[0] || !args[1]) throw ValueError() << "Expected 2 arguments";
  DataTable* dt = args[0].to_datatable();
  size_t col    = args[1].to_size_t();

  if (!dt) throw TypeError() << "First parameter should be a Frame";
  if (col >= dt->ncols) throw ValueError() << "Index out of bounds";
  return std::make_pair(dt, col);
}


static py::PKArgs args_frame_column_rowindex(
    2, 0, 0, false, false, {"frame", "i"},
    "frame_column_rowindex",
R"(frame_column_rowindex(frame, i)
--

Return the RowIndex of the `i`th column of the `frame`, or None if that column
has no row index.
)");

static py::oobj frame_column_rowindex(const py::PKArgs& args) {
  auto u = _unpack_frame_column_args(args);
  DataTable* dt = u.first;
  size_t col = u.second;

  RowIndex ri = dt->columns[col]->rowindex();
  return ri? py::orowindex(ri) : py::None();
}


static py::PKArgs args_frame_column_data_r(
    2, 0, 0, false, false, {"frame", "i"},
    "frame_column_data_r",
R"(frame_column_data_r(frame, i)
--

Return C pointer to the main data array of the column `frame[i]`. The pointer
is returned as a `ctypes.c_void_p` object.
)");

static py::oobj frame_column_data_r(const py::PKArgs& args) {
  static py::oobj c_void_p = py::oobj::import("ctypes", "c_void_p");

  auto u = _unpack_frame_column_args(args);
  DataTable* dt = u.first;
  size_t col = u.second;
  size_t iptr = reinterpret_cast<size_t>(dt->columns[col]->data());
  return c_void_p.call({py::oint(iptr)});
}


static py::PKArgs args_frame_integrity_check(
  1, 0, 0, false, false, {"frame"}, "frame_integrity_check",
R"(frame_integrity_check(frame)
--

This function performs a range of tests on the `frame` to verify
that its internal state is consistent. It returns None on success,
or throws an AssertionError if any problems were found.
)");

static void frame_integrity_check(const py::PKArgs& args) {
  if (!args[0].is_frame()) {
    throw TypeError() << "Function `frame_integrity_check()` takes a Frame "
        "as a single positional argument";
  }
  auto frame = static_cast<py::Frame*>(args[0].to_borrowed_ref());
  frame->integrity_check();
}



static py::PKArgs args_in_debug_mode(
    0, 0, 0, false, false, {}, "in_debug_mode",
    "Return True if datatable was compiled in debug mode");

static py::oobj in_debug_mode(const py::PKArgs&) {
  #ifdef DTDEBUG
    return py::True();
  #else
    return py::False();
  #endif
}



static py::PKArgs args_has_omp_support(
    0, 0, 0, false, false, {}, "has_omp_support",
R"(Return True if datatable was built with OMP support, and False otherwise.
Without OMP datatable will be significantly slower, performing all
operations in single-threaded mode.
)");

static py::oobj has_omp_support(const py::PKArgs&) {
  #ifdef DTNOOPENMP
    return py::False();
  #else
    return py::True();
  #endif
}


static py::PKArgs args_get_thread_ids(
    0, 0, 0, false, false, {}, "get_thread_ids",
R"(Return system ids of all threads used internally by datatable)");

static py::oobj get_thread_ids(const py::PKArgs&) {
  std::mutex m;
  size_t n = dt::get_num_threads();
  py::olist list(n);
  xassert(dt::get_thread_num() == size_t(-1));

  dt::parallel_region([&](size_t i) {
    std::stringstream ss;
    ss << std::this_thread::get_id();
    std::lock_guard<std::mutex> lock(m);
    list.set(i, py::ostring(ss.str()));
    xassert(i == dt::get_thread_num());
  });

  for (size_t i = 0; i < n; ++i) {
    xassert(list[i]);
  }
  return std::move(list);
}




static py::PKArgs args__register_function(
    2, 0, 0, false, false, {"n", "fn"}, "_register_function", nullptr);

static void _register_function(const py::PKArgs& args) {
  size_t n = args.get<size_t>(0);
  py::oobj fn = args[1].to_oobj();

  PyObject* fnref = std::move(fn).release();
  switch (n) {
    case 2: init_py_stype_objs(fnref); break;
    case 3: init_py_ltype_objs(fnref); break;
    case 4: replace_typeError(fnref); break;
    case 5: replace_valueError(fnref); break;
    case 6: replace_dtWarning(fnref); break;
    case 7: py::Frame_Type = fnref; break;
    case 8: py::fread_fn = fnref; break;
    default: throw ValueError() << "Unknown index: " << n;
  }
}



static py::PKArgs args__column_save_to_disk(
  4, 0, 0, false, false,
  {"frame", "i", "filename", "strategy"},
  "_column_save_to_disk",
  "Save `frame[i]` column's data into the file `filename`,\n"
  "using the provided writing strategy.\n");

static void _column_save_to_disk(const py::PKArgs& args) {
  DataTable* dt        = args[0].to_datatable();
  size_t i             = args[1].to_size_t();
  std::string filename = args[2].to_string();
  std::string strategy = args[3].to_string();

  Column* col = dt->columns[i];
  auto sstrategy = (strategy == "mmap")  ? WritableBuffer::Strategy::Mmap :
                   (strategy == "write") ? WritableBuffer::Strategy::Write :
                                           WritableBuffer::Strategy::Auto;

  col->save_to_disk(filename, sstrategy);
}



//------------------------------------------------------------------------------
// Support memory leak detection
//------------------------------------------------------------------------------
#ifdef DTDEBUG

struct PtrInfo {
  size_t alloc_size;
  const char* name;

  std::string to_string() {
    std::ostringstream io;
    io << name << "[" << alloc_size << "]";
    return io.str();
  }
};

static std::unordered_map<void*, PtrInfo> tracked_objects;
static std::mutex track_mutex;


void TRACK(void* ptr, size_t size, const char* name) {
  std::lock_guard<std::mutex> lock(track_mutex);

  if (tracked_objects.count(ptr)) {
    std::cerr << "ERROR: Pointer " << ptr << " is already tracked. Old "
        "pointer contains " << tracked_objects[ptr].to_string() << ", new: "
        << (PtrInfo {size, name}).to_string();
  }
  tracked_objects.insert({ptr, PtrInfo {size, name}});
}


void UNTRACK(void* ptr) {
  std::lock_guard<std::mutex> lock(track_mutex);

  if (tracked_objects.count(ptr) == 0) {
    // UNTRACK() is usually called from a destructor, so cannot throw any
    // exceptions there :(
    std::cerr << "ERROR: Trying to remove pointer " << ptr
              << " which is not tracked\n";
  }
  tracked_objects.erase(ptr);
}


bool IS_TRACKED(void* ptr) {
  std::lock_guard<std::mutex> lock(track_mutex);
  return (tracked_objects.count(ptr) > 0);
}



static py::PKArgs args_get_tracked_objects(
    0, 0, 0, false, false, {}, "get_tracked_objects", nullptr);

static py::oobj get_tracked_objects(const py::PKArgs&) {
  py::odict res;
  for (auto kv : tracked_objects) {
    res.set(py::oint(reinterpret_cast<size_t>(kv.first)),
            py::ostring(kv.second.to_string()));
  }
  return std::move(res);
}

#endif


//------------------------------------------------------------------------------
// Module definition
//------------------------------------------------------------------------------

void py::DatatableModule::init_methods() {
  ADD_FN(&_register_function, args__register_function);
  ADD_FN(&has_omp_support, args_has_omp_support);
  ADD_FN(&in_debug_mode, args_in_debug_mode);
  ADD_FN(&frame_column_rowindex, args_frame_column_rowindex);
  ADD_FN(&frame_column_data_r, args_frame_column_data_r);
  ADD_FN(&_column_save_to_disk, args__column_save_to_disk);
  ADD_FN(&frame_integrity_check, args_frame_integrity_check);
  ADD_FN(&get_thread_ids, args_get_thread_ids);

  init_methods_aggregate();
  init_methods_buffers();
  init_methods_cbind();
  init_methods_csv();
  init_methods_jay();
  init_methods_join();
  init_methods_kfold();
  init_methods_nff();
  init_methods_options();
  init_methods_rbind();
  init_methods_repeat();
  init_methods_sets();
  init_methods_str();

  init_casts();

  #ifdef DTTEST
    init_tests();
  #endif
  #ifdef DTDEBUG
    ADD_FN(&get_tracked_objects, args_get_tracked_objects);
  #endif
}


/* Called when Python program imports the module */
PyMODINIT_FUNC PyInit__datatable() noexcept
{
  static py::DatatableModule dtmod;
  PyObject* m = nullptr;

  try {
    init_exceptions();

    m = dtmod.init();

    // Initialize submodules
    if (!init_py_encodings(m)) return nullptr;

    init_types();
    expr::init_reducers();

    py::Frame::Type::init(m);
    py::Ftrl::Type::init(m);
    py::base_expr::Type::init(m);
    py::orowindex::pyobject::Type::init(m);
    py::oby::init(m);
    py::ojoin::init(m);
    py::osort::init(m);

  } catch (const std::exception& e) {
    exception_to_python(e);
    m = nullptr;
  }

  return m;
}
