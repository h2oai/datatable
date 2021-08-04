//------------------------------------------------------------------------------
// Copyright 2018-2021 H2O.ai
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
#include "call_logger.h"
#include "csv/reader.h"
#include "datatablemodule.h"
#include "documentation.h"
#include "expr/fexpr.h"
#include "expr/head_func.h"
#include "expr/head_reduce.h"
#include "expr/namespace.h"
#include "expr/py_by.h"              // py::oby
#include "expr/py_join.h"            // py::ojoin
#include "expr/py_sort.h"            // py::osort
#include "expr/py_update.h"          // py::oupdate
#include "frame/py_frame.h"
#include "frame/repr/html_widget.h"
#include "ltype.h"
#include "models/aggregate.h"
#include "models/py_ftrl.h"
#include "models/py_linearmodel.h"
#include "options.h"
#include "parallel/api.h"
#include "parallel/thread_pool.h"
#include "progress/_options.h"
#include "py_encodings.h"
#include "python/_all.h"
#include "python/string.h"
#include "python/xargs.h"
#include "read/py_read_iterator.h"
#include "sort.h"
#include "types/py_type.h"
#include "utils/assert.h"
#include "utils/exceptions.h"
#include "utils/macros.h"
#include "utils/terminal/terminal.h"
#include "utils/terminal/terminal_stream.h"
#include "utils/terminal/terminal_style.h"



static_assert(INTPTR_MAX == INT64_MAX,
              "Only 64 bit platforms are supported.");

static_assert(sizeof(void*) == 8, "Expected size(void*) to be 8 bytes");
static_assert(sizeof(void*) == sizeof(size_t),
              "size(size_t) != size(void*)");
static_assert(sizeof(void*) == sizeof(int64_t),
              "size(int64_t) != size(void*)");

static_assert(sizeof(int8_t) == 1, "int8_t should be 1-byte");
static_assert(sizeof(int16_t) == 2, "int16_t should be 2-byte");
static_assert(sizeof(int32_t) == 4, "int32_t should be 4-byte");
static_assert(sizeof(int64_t) == 8, "int64_t should be 8-byte");
static_assert(sizeof(float) == 4, "float should be 4-byte");
static_assert(sizeof(double) == 8, "double should be 8-byte");
static_assert(sizeof(char) == sizeof(unsigned char), "char != uchar");
static_assert(sizeof(char) == 1, "sizeof(char) != 1");

static_assert(sizeof(dt::LType) == 1, "LType does not fit in a byte");
static_assert(sizeof(dt::SType) == 1, "SType does not fit in a byte");

static_assert(static_cast<unsigned>(-1) - static_cast<unsigned>(-3) == 2,
              "Unsigned arithmetics check");
static_assert(3u - (0-1u) == 4u, "Unsigned arithmetics check");
static_assert(0-1u == 0xFFFFFFFFu, "Unsigned arithmetics check");

static_assert(sizeof(int64_t) == sizeof(Py_ssize_t),
              "int64_t and Py_ssize_t should refer to the same type");




//------------------------------------------------------------------------------
// These functions are exported as `datatable.internal.*`
//------------------------------------------------------------------------------

static std::pair<DataTable*, size_t>
_unpack_frame_column_args(const py::XArgs& args)
{
  if (!args[0] || !args[1]) throw ValueError() << "Expected 2 arguments";
  DataTable* dt = args[0].to_datatable();
  size_t col    = args[1].to_size_t();

  if (!dt) throw TypeError() << "First parameter should be a Frame";
  if (col >= dt->ncols()) throw ValueError() << "Index out of bounds";
  return std::make_pair(dt, col);
}


static py::oobj frame_columns_virtual(const py::XArgs& args) {
  DataTable* dt = args[0].to_datatable();
  py::olist virtuals(dt->ncols());
  for (size_t i = 0; i < dt->ncols(); ++i) {
    virtuals.set(i, py::obool(dt->get_column(i).is_virtual()));
  }
  return std::move(virtuals);
}

DECLARE_PYFN(&frame_columns_virtual)
    ->name("frame_columns_virtual")
    ->docs(dt::doc_internal_frame_columns_virtual)
    ->n_positional_args(1)
    ->n_required_args(1)
    ->arg_names({"frame"});



static py::oobj frame_column_data_r(const py::XArgs& args) {
  static py::oobj c_void_p = py::oobj::import("ctypes", "c_void_p");

  auto u = _unpack_frame_column_args(args);
  DataTable* dt = u.first;
  size_t col_index = u.second;
  Column& col = dt->get_column(col_index);
  col.materialize();  // Needed for getting the column's data buffer
  size_t iptr = reinterpret_cast<size_t>(col.get_data_readonly());
  return c_void_p.call({py::oint(iptr)});
}

DECLARE_PYFN(&frame_column_data_r)
    ->name("frame_column_data_r")
    ->docs(dt::doc_internal_frame_column_data_r)
    ->n_positional_args(2)
    ->n_required_args(2)
    ->arg_names({"frame", "i"});



static py::oobj frame_integrity_check(const py::XArgs& args) {
  if (!args[0].is_frame()) {
    throw TypeError() << "Function `frame_integrity_check()` takes a Frame "
        "as a single positional argument";
  }
  auto frame = static_cast<py::Frame*>(args[0].to_borrowed_ref());
  frame->integrity_check();
  return py::None();
}

DECLARE_PYFN(&frame_integrity_check)
    ->name("frame_integrity_check")
    ->docs(dt::doc_internal_frame_integrity_check)
    ->n_positional_args(1)
    ->n_required_args(1)
    ->arg_names({"frame"});



static py::oobj get_thread_ids(const py::XArgs&) {
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

DECLARE_PYFN(&get_thread_ids)
    ->name("get_thread_ids")
    ->docs(dt::doc_internal_get_thread_ids);




static py::oobj _register_function(const py::XArgs& args) {
  size_t n = args[0].to_size_t();
  py::oobj fn = args[1].to_oobj();

  PyObject* fnref = std::move(fn).release();
  switch (n) {
    case 2: dt::init_py_stype_objs(fnref); break;
    case 3: dt::init_py_ltype_objs(fnref); break;
    case 7: py::Frame_Type = fnref; break;
    case 9: py::Expr_Type = fnref; break;
    default: throw ValueError() << "Unknown index: " << n;
  }
  return py::None();
}

DECLARE_PYFN(&_register_function)
    ->name("_register_function")
    ->n_positional_args(2)
    ->n_required_args(2)
    ->arg_names({"n", "fn"});


// This is a private function, used from build_info.py only.
static py::oobj compiler_version(const py::XArgs&) {
  #define STR(x) STR1(x)
  #define STR1(x) #x
  const char* compiler =
    #if DT_COMPILER_CLANG
      "CLang " STR(__clang_major__) "." STR(__clang_minor__) "." STR(__clang_patchlevel__);
    #elif DT_COMPILER_MSVC
      "MSVC " STR(_MSC_FULL_VER);
    #elif defined(__MINGW64__)
      "MinGW64 " STR(__MINGW64_VERSION_MAJOR) "." STR(__MINGW64_VERSION_MINOR);
    #elif DT_COMPILER_GCC
      "GCC " STR(__GNUC__) "." STR(__GNUC_MINOR__) "." STR(__GNUC_PATCHLEVEL__);
    #else
      "Unknown";
    #endif
  return py::ostring(compiler);
}

DECLARE_PYFN(&compiler_version)
    ->name("_compiler");



static py::oobj apply_color(const py::XArgs& args) {
  if (args[0].is_none_or_undefined()) {
    throw TypeError() << "Missing required argument `color`";
  }
  if (args[1].is_none_or_undefined()) {
    throw TypeError() << "Missing required argument `text`";
  }
  bool use_colors = dt::Terminal::standard_terminal().colors_enabled();
  if (!use_colors) {
    return args[1].to_oobj();
  }
  std::string color = args[0].to_string();
  std::string text = args[1].to_string();

  dt::TerminalStream ts(true);
  if      (color == "bright_black") ts << dt::style::grey;
  else if (color == "grey"        ) ts << dt::style::grey;
  else if (color == "bright_green") ts << dt::style::bgreen;
  else if (color == "dim"         ) ts << dt::style::dim;
  else if (color == "italic"      ) ts << dt::style::italic;
  else if (color == "yellow"      ) ts << dt::style::yellow;
  else if (color == "bold"        ) ts << dt::style::bold;
  else if (color == "red"         ) ts << dt::style::red;
  else if (color == "bright_red"  ) ts << dt::style::bred;
  else if (color == "cyan"        ) ts << dt::style::cyan;
  else if (color == "bright_cyan" ) ts << dt::style::bcyan;
  else {
    throw ValueError() << "Unknown color `" << color << "`";
  }
  ts << text << dt::style::end;
  return py::ostring(ts.str());
}

DECLARE_PYFN(&apply_color)
    ->name("apply_color")
    ->docs("Paint the text into the specified color with by appending "
           "the appropriate terminal control sequences")
    ->n_positional_args(2)
    ->n_required_args(2)
    ->arg_names({"color", "text"});



static py::oobj initialize_options(const py::XArgs& args) {
  py::oobj options = args[0].to_oobj();
  if (options) {
    dt::use_options_store(options);
    dt::ThreadPool::init_options();
    dt::progress::init_options();
    py::Frame::init_names_options();
    py::Frame::init_display_options();
    dt::read::GenericReader::init_options();
    sort_init_options();
    dt::CallLogger::init_options();
  }
  return py::None();
}

DECLARE_PYFN(&initialize_options)
    ->name("initialize_options")
    ->docs("Signal to core C++ datatable to register all internal options\n"
           "with the provided options manager.")
    ->n_positional_args(1)
    ->n_required_args(1)
    ->arg_names({"options"});



static py::oobj initialize_final(const py::XArgs&) {
  init_exceptions();
  return py::None();
}

DECLARE_PYFN(&initialize_final)
    ->name("initialize_final")
    ->docs("Called once at the end of initialization of the python datatable "
           "module. This function will import some of the objects defined "
           "in the python module into the extension.");




//------------------------------------------------------------------------------
// Module definition
//------------------------------------------------------------------------------

void py::DatatableModule::init_methods() {
  for (py::XArgs* xarg : py::XArgs::store()) {
    if (xarg->get_class_id() == 0) {
      add(xarg->get_method_def());
    }
  }

  init_methods_jay();
  init_methods_join();

  init_fbinary();
  init_funary();
  init_fuzzy();
}


extern "C" {

  /* Called when Python program imports the module */
  PyMODINIT_FUNC PyInit__datatable() noexcept
  {
    static py::DatatableModule dtmod;
    PyObject* m = nullptr;

    try {
      m = dtmod.init();

      // Initialize submodules
      if (!init_py_encodings(m)) return nullptr;

      dt::expr::Head_Func::init();

      py::Frame::init_type(m);
      py::Ftrl::init_type(m);
      py::LinearModel::init_type(m);
      py::ReadIterator::init_type(m);
      py::Namespace::init_type(m);
      dt::expr::PyFExpr::init_type(m);
      dt::PyType::init_type(m);

      dt::init_config_option(m);
      py::oby::init(m);
      py::ojoin::init(m);
      py::osort::init(m);
      py::oupdate::init(m);
      py::datetime_init();

    } catch (const std::exception& e) {
      exception_to_python(e);
      m = nullptr;
    }

    return m;
  }

} // extern "C"

