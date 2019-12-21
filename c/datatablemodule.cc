//------------------------------------------------------------------------------
// Copyright 2018-2019 H2O.ai
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the "Software"),
// to deal in the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense,
// and/or sell copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.
//------------------------------------------------------------------------------
#include <exception>       // std::exception
#include <iostream>        // std::cerr
#include <mutex>           // std::mutex, std::lock_guard
#include <thread>          // std::this_thread
#include <sstream>         // std::stringstream
#include <unordered_map>   // std::unordered_map
#include <utility>         // std::pair, std::make_pair, std::move
#include "../datatable/include/datatable.h"
#include "csv/reader.h"
#include "expr/head_func.h"
#include "expr/head_reduce.h"
#include "expr/py_by.h"              // py::oby
#include "expr/py_join.h"            // py::ojoin
#include "expr/py_sort.h"            // py::osort
#include "expr/py_update.h"          // py::oupdate
#include "frame/py_frame.h"
#include "frame/repr/html_widget.h"
#include "models/aggregator.h"
#include "models/py_ftrl.h"
#include "parallel/api.h"
#include "parallel/thread_pool.h"
#include "progress/_options.h"
#include "python/_all.h"
#include "python/string.h"
#include "utils/assert.h"
#include "utils/macros.h"
#include "utils/terminal/terminal.h"
#include "datatablemodule.h"
#include "options.h"
#include "sort.h"
#include "py_encodings.h"
#include "ztest.h"



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
  if (col >= dt->ncols()) throw ValueError() << "Index out of bounds";
  return std::make_pair(dt, col);
}


static py::PKArgs args_frame_columns_virtual(
    1, 0, 0, false, false, {"frame"},
    "frame_columns_virtual",
R"(frame_columns_virtual(frame)
--

Return the tuple of which columns in the Frame are virtual.
)");

static py::oobj frame_columns_virtual(const py::PKArgs& args) {
  DataTable* dt = args[0].to_datatable();
  py::otuple virtuals(dt->ncols());
  for (size_t i = 0; i < dt->ncols(); ++i) {
    virtuals.set(i, py::obool(dt->get_column(i).is_virtual()));
  }
  return std::move(virtuals);
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
  size_t iptr = reinterpret_cast<size_t>(
                    dt->get_column(col).get_data_readonly());
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
  #if DTDEBUG
    return py::True();
  #else
    return py::False();
  #endif
}



static py::PKArgs args_get_thread_ids(
    0, 0, 0, false, false, {}, "get_thread_ids",
R"(Return system ids of all threads used internally by datatable)");

static py::oobj get_thread_ids(const py::PKArgs&) {
  std::mutex m;
  size_t n = dt::num_threads_in_pool();
  py::olist list(n);
  xassert(dt::this_thread_index() == 0);

  dt::parallel_region([&] {
    std::stringstream ss;
    size_t i = dt::this_thread_index();
    ss << std::this_thread::get_id();
    std::lock_guard<std::mutex> lock(m);
    xassert(!list[i]);
    list.set(i, py::ostring(ss.str()));
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
    case 9: py::Expr_Type = fnref; break;
    default: throw ValueError() << "Unknown index: " << n;
  }
}



static py::PKArgs args_compiler_version(
  0, 0, 0, false, false, {}, "compiler_version",
  "Return the version of the C++ compiler used to compile this module");

const char* get_compiler_version_string() {
  #define STR(x) STR1(x)
  #define STR1(x) #x
  #ifdef __clang__
    return "CLang " STR(__clang_major__) "." STR(__clang_minor__) "."
           STR(__clang_patchlevel__);
  #elif defined(_MSC_VER)
    return "MSVC " STR(_MSC_FULL_VER);
  #elif defined(__MINGW64__)
    return "MinGW64 " STR(__MINGW64_VERSION_MAJOR) "."
           STR(__MINGW64_VERSION_MINOR);
  #elif defined(__GNUC__)
    return "GCC " STR(__GNUC__) "." STR(__GNUC_MINOR__) "."
           STR(__GNUC_PATCHLEVEL__);
  #else
    return "Unknown";
  #endif
  #undef STR
  #undef STR1
}

static py::oobj compiler_version(const py::PKArgs&) {
  return py::ostring(get_compiler_version_string());
}



static py::PKArgs args_regex_supported(
  0, 0, 0, false, false, {}, "regex_supported",
  "Was the datatable built with regular expression support?");

static py::oobj regex_supported(const py::PKArgs&) {
  return py::obool(REGEX_SUPPORTED);
}




static py::PKArgs args_initialize_options(
  1, 0, 0, false, false, {"options"}, "initialize_options",
  "Signal to core C++ datatable to register all internal options\n"
  "with the provided options manager.");

static void initialize_options(const py::PKArgs& args) {
  py::oobj options = args[0].to_oobj();
  if (!options) return;

  dt::use_options_store(options);
  dt::thread_pool::init_options();
  dt::progress::init_options();
  py::Frame::init_names_options();
  py::Frame::init_display_options();
  GenericReader::init_options();
  sort_init_options();
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
    // exceptions there. However, calling PyErr_*() will cause the python
    // to raise SystemError once the control reaches python.
    PyErr_SetString(PyExc_RuntimeError,
                    "ERROR: Trying to remove pointer which is not tracked");
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
  ADD_FN(&in_debug_mode, args_in_debug_mode);
  ADD_FN(&frame_columns_virtual, args_frame_columns_virtual);
  ADD_FN(&frame_column_data_r, args_frame_column_data_r);
  ADD_FN(&frame_integrity_check, args_frame_integrity_check);
  ADD_FN(&get_thread_ids, args_get_thread_ids);
  ADD_FN(&initialize_options, args_initialize_options);
  ADD_FN(&compiler_version, args_compiler_version);
  ADD_FN(&regex_supported, args_regex_supported);

  init_methods_aggregate();
  init_methods_buffers();
  init_methods_cbind();
  init_methods_csv();
  init_methods_isclose();
  init_methods_jay();
  init_methods_join();
  init_methods_kfold();
  init_methods_rbind();
  init_methods_repeat();
  init_methods_sets();
  init_methods_shift();
  init_methods_str();
  init_methods_styles();
  init_methods_zread();

  init_casts();
  init_fbinary();
  init_fnary();
  init_funary();
  init_fuzzy();

  #ifdef DTTEST
    init_tests();
  #endif
  #ifdef DTDEBUG
    ADD_FN(&get_tracked_objects, args_get_tracked_objects);
  #endif
}


extern "C" {

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
      dt::Terminal::standard_terminal().initialize();

      init_types();
      dt::expr::Head_Func::init();

      py::Frame::init_type(m);
      py::Ftrl::init_type(m);
      dt::init_config_option(m);
      py::oby::init(m);
      py::ojoin::init(m);
      py::osort::init(m);
      py::oupdate::init(m);

    } catch (const std::exception& e) {
      exception_to_python(e);
      m = nullptr;
    }

    return m;
  }

} // extern "C"

