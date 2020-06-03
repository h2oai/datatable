//------------------------------------------------------------------------------
// Copyright 2020 H2O.ai
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
#include <chrono>
#include <iostream>
#include "call_logger.h"
#include "options.h"
#include "python/args.h"
#include "python/bool.h"
#include "python/string.h"
#include "utils/logger.h"   // dt::log::Logger
namespace dt {

// LOG must be allocated dynamically, to prevent it from being
// destroyed at program exit after python runtime has already shut
// down (which will result in a crash, since Logger tries to DECREF
// a PyObject that it owns).
//
static log::Logger* LOG = new log::Logger;

// Number of `CallLogger::Impl`s in the cache. Normally we shouldn't
// see calls that are more than 2 levels deep, but who knows what
// the future will hold.
//
static constexpr size_t N_IMPLS = 10;



//------------------------------------------------------------------------------
// Options
//------------------------------------------------------------------------------

static bool opt_report_args = false;

static void _init_options() {
  register_option(
    "debug.logger",
    [] {
      return LOG->get_pylogger(true);
    },
    [](const py::Arg& arg) {
      if (arg.is_none()) {             // debug.logger = None
        LOG->disable();
      }
      else if (arg.is_string()) {
        auto arg_str = arg.to_string();
        if (arg_str == "default") {    // debug.logger = "default"
          LOG->enable();
        } else {                       // debug.logger = <some other string>
          throw ValueError()
              << "Invalid value for `" << arg.name() << "`: "  << arg_str;
        }
      }
      else {                           // debug.logger = <object>
        LOG->use_pylogger(arg.to_oobj());
      }
    },
    "The logger object used for reporting calls to datatable core\n"
    "functions. If None, then logging is disabled (this is the\n"
    "default).\n\n"
    "Setting this option to 'default' will use the built-in\n"
    "datatable logger. Otherwise you can use any other object\n"
    "that implements a `.debug(msg)` method.\n"
  );
}




//------------------------------------------------------------------------------
// CallLogger::Impl
//------------------------------------------------------------------------------

enum class CallType : size_t {
  UNKNOWN = 0,
  FUNCTION = 1,
  METHOD = 2,
  DEALLOC = 3,
  GET = 4,
  SET = 5,
  GETITEM = 6,
  SETITEM = 7,
};


class CallLogger::Impl
{
  using stime_t = std::chrono::time_point<std::chrono::steady_clock>;
  private:
    std::string       indent_;
    CallType          type_;
    const py::PKArgs* pkargs_;
    const PyObject*   pythis_;
    const PyObject*   pyargs_;
    const PyObject*   pykwds_;
    const PyObject*   pyvalue_;
    const py::GSArgs* gsargs_;
    stime_t           t_start_;
    bool              header_printed_;
    size_t : 56;

  public:
    Impl(size_t i);
    void init_function(const py::PKArgs* pkargs, PyObject* pyargs, PyObject* pykwds) noexcept;
    void init_method  (const py::PKArgs* pkargs, PyObject* pythis, PyObject* pyargs, PyObject* pykwds) noexcept;
    void init_dealloc (PyObject* pythis) noexcept;
    void init_getter  (PyObject* pythis, void* closure) noexcept;
    void init_getitem (PyObject* pythis, PyObject* key) noexcept;
    void init_setitem (PyObject* pythis, PyObject* key, PyObject* val) noexcept;

    void emit_header() noexcept;
    void finish() noexcept;

  private:
    void init_common() noexcept;

    void print_name(log::Message&);
    void print_arguments(log::Message&);
    void print_result(log::Message&, double elapsed_time);
};



//---- Constructors ------------------------------------------------------------

CallLogger::Impl::Impl(size_t i)
    : indent_(2*i, ' ') {}


void CallLogger::Impl::init_common() noexcept {
  t_start_ = std::chrono::steady_clock::now();
  header_printed_ = false;
}


void CallLogger::Impl::init_function(
    const py::PKArgs* pkargs, PyObject* pyargs, PyObject* pykwds) noexcept
{
  type_ = CallType::FUNCTION;
  pkargs_ = pkargs;
  pyargs_ = pyargs;
  pykwds_ = pykwds;
  init_common();
}


void CallLogger::Impl::init_method(
    const py::PKArgs* pkargs, PyObject* pythis, PyObject* pyargs,
    PyObject* pykwds) noexcept
{
  type_ = CallType::METHOD;
  pkargs_ = pkargs;
  pythis_ = pythis;
  pyargs_ = pyargs;
  pykwds_ = pykwds;
  init_common();
}


void CallLogger::Impl::init_dealloc(PyObject* pythis) noexcept {
  type_ = CallType::DEALLOC;
  pythis_ = pythis;
  init_common();
}


void CallLogger::Impl::init_getter(PyObject* pythis, void* closure) noexcept {
  type_ = CallType::GET;
  pythis_ = pythis;
  gsargs_ = static_cast<const py::GSArgs*>(closure);
  init_common();
}


void CallLogger::Impl::init_getitem(PyObject* pythis, PyObject* key) noexcept {
  type_ = CallType::GETITEM;
  pythis_ = pythis;
  pyargs_ = key;
  init_common();
}


void CallLogger::Impl::init_setitem(
    PyObject* pythis, PyObject* key, PyObject* val) noexcept
{
  type_ = CallType::SETITEM;
  pythis_ = pythis;
  pyargs_ = key;
  pyvalue_ = val;
  init_common();
}



//---- Display -----------------------------------------------------------------

void CallLogger::Impl::emit_header() noexcept {
  if (header_printed_) return;
  try {
    auto msg = LOG->info();
    print_name(msg);
    msg << " {";
    header_printed_ = true;
  } catch (...) {
    std::cerr << "... log failed\n";  // LCOV_EXCL_LINE
  }
}


void CallLogger::Impl::finish() noexcept {
  if (!LOG) return;
  try {
    stime_t t_end = std::chrono::steady_clock::now();
    std::chrono::duration<double> diff = t_end - t_start_;
    auto msg = LOG->info();
    msg << indent_;
    if (header_printed_) {
      msg << '}';
    } else {
      print_name(msg);
    }
    print_result(msg, diff.count());
    type_ = CallType::UNKNOWN;
  } catch (...) {
    std::cerr << "... log failed\n";  // LCOV_EXCL_LINE
  }
}


void CallLogger::Impl::print_name(log::Message& out) {
  switch (type_) {
    case CallType::FUNCTION: {
      out << "dt." << pkargs_->get_short_name() << '(';
      print_arguments(out);
      out << ')';
      break;
    }
    case CallType::METHOD: {
      py::ostring this_repr = py::robj(pythis_).repr();
      out << this_repr.to_cstring() << '.' << pkargs_->get_short_name() << '(';
      print_arguments(out);
      out << ')';
      break;
    }
    case CallType::DEALLOC: {
      py::ostring this_repr = py::robj(pythis_).repr();
      out << this_repr.to_cstring() << ".__del__()";
      break;
    }
    case CallType::GET: {
      py::ostring this_repr = py::robj(pythis_).repr();
      out << this_repr.to_cstring() << '.' << gsargs_->name;
      break;
    }
    case CallType::GETITEM:
    case CallType::SETITEM: {
      py::ostring this_repr = py::robj(pythis_).repr();
      out << this_repr.to_cstring() << '[';
      print_arguments(out);
      out << ']';
      if (type_ == CallType::SETITEM) {
        out << " = ?";
      }
      break;
    }
    default: {
      out << "<unknown>";
      break;
    }
  }
}


void CallLogger::Impl::print_arguments(log::Message& out) {
  (void)opt_report_args;
  (void)out;
}


void CallLogger::Impl::print_result(log::Message& out, double elapsed_time) {
  out << " # "
      << (PyErr_Occurred()? "failed" : "done")
      << " in " << elapsed_time << " s";
}




//------------------------------------------------------------------------------
// CallLogger main class
//------------------------------------------------------------------------------

std::vector<CallLogger::Impl*> CallLogger::impl_cache_;
size_t CallLogger::nested_level_ = 0;


CallLogger::CallLogger() noexcept {
  impl_ = nullptr;
  if (LOG->enabled()) {
    if (nested_level_ < impl_cache_.size()) {
      if (nested_level_ > 0) {
        impl_cache_[nested_level_ - 1]->emit_header();
      }
      impl_ = impl_cache_[nested_level_++];
    }
    else if (nested_level_) {
      std::cerr << "nested call too deep\n";  // LCOV_EXCL_LINE
    }
  }
}


CallLogger CallLogger::function(
    const py::PKArgs* pkargs, PyObject* pyargs, PyObject* pykwds) noexcept
{
  CallLogger cl;
  if (cl.impl_) cl.impl_->init_function(pkargs, pyargs, pykwds);
  return cl;
}


CallLogger CallLogger::method(const py::PKArgs* pkargs,
    PyObject* pythis, PyObject* pyargs, PyObject* pykwds) noexcept
{
  CallLogger cl;
  if (cl.impl_) cl.impl_->init_method(pkargs, pythis, pyargs, pykwds);
  return cl;
}


CallLogger CallLogger::dealloc(PyObject* pythis) noexcept {
  CallLogger cl;
  if (cl.impl_) cl.impl_->init_dealloc(pythis);
  return cl;
}

CallLogger CallLogger::getter(PyObject* pythis, void* closure) noexcept {
  CallLogger cl;
  if (cl.impl_) cl.impl_->init_getter(pythis, closure);
  return cl;
}

CallLogger CallLogger::getitem(PyObject* pythis, PyObject* key) noexcept {
  CallLogger cl;
  if (cl.impl_) cl.impl_->init_getitem(pythis, key);
  return cl;
}

CallLogger CallLogger::setitem(PyObject* pythis, PyObject* key, PyObject* val) noexcept {
  CallLogger cl;
  if (cl.impl_) cl.impl_->init_setitem(pythis, key, val);
  return cl;
}


CallLogger::CallLogger(CallLogger&& other) noexcept {
  impl_ = other.impl_;
  other.impl_ = nullptr;
}


CallLogger::~CallLogger() {
  if (impl_) {
    impl_->finish();
    nested_level_--;
  }
}


void CallLogger::init_options() {
  CallLogger::impl_cache_.resize(N_IMPLS);
  for (size_t i = 0; i < N_IMPLS; ++i) {
    CallLogger::impl_cache_[i] = new CallLogger::Impl(i);
  }
  _init_options();
}




}  // namespace dt
