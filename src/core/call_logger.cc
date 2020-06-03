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

static bool opt_report_args = true;
static size_t opt_truncate_length = 100;

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

  register_option(
    "debug.report_args",
    [] {
      return py::obool(opt_report_args);
    },
    [](const py::Arg& arg) {
      opt_report_args = arg.to_bool_strict();
    },
    "Controls whether log messages about function and method calls\n"
    "contain information about the arguments of those calls."
  );

  register_option(
    "debug.arg_max_size",
    [] {
      return py::oint(opt_truncate_length);
    },
    [](const py::Arg& arg) {
      opt_truncate_length = std::min(arg.to_size_t(), size_t(10));
    },
    "When the `report_args` is on, this option will limit the\n"
    "display size of each argument in order to prevent potentially\n"
    "huge outputs. This option's value cannot be less than 10."
  );
}




//------------------------------------------------------------------------------
// Value printing
//------------------------------------------------------------------------------

struct R {
  py::robj obj;
  R(py::robj t) : obj(t) {}
};

template <>
log::Message& log::Message::operator<<(const R& r) {
  if (r.obj.is_slice()) {
    auto sliceobj = r.obj.to_oslice();
    if (sliceobj.is_numeric()) {
      int64_t start = sliceobj.start();
      int64_t stop = sliceobj.stop();
      int64_t step = sliceobj.step();
      if (start != py::oslice::NA) out_ << start;
      out_ << ':';
      if (stop != py::oslice::NA) out_ << stop;
      if (step != py::oslice::NA) out_ << ':' << step;
      return *this;
    }
  }
  py::ostring repr = r.obj.repr();
  auto strobj = repr.to_cstring();
  if (static_cast<size_t>(strobj.size) <= opt_truncate_length) {
    out_.write(strobj.ch, strobj.size);
  } else {
    auto len0 = static_cast<long>(opt_truncate_length * 3/5);
    auto len1 = static_cast<long>(opt_truncate_length * 2/5 - 3);
    out_.write(strobj.ch, len0);
    out_.write("...", 3);
    out_.write(strobj.ch + strobj.size - len1, len1);
  }
  return *this;
}




//------------------------------------------------------------------------------
// CallLogger::Impl
//------------------------------------------------------------------------------

/**
  * Main class that implements the "call logging" functionality.
  * Instances of this class are constructed at module startup and
  * then reused when needed (see CallLogger::impl_cache_).
  *
  * One of the `impl_***()` methods must be called when an object
  * gets reused, and then call `finish()` at the end.
  *
  * ---- implementation details ----
  *
  * out_ is the log::Message instance that will be sent to logger
  *     once the call is complete. There are 2 modes how this can
  *     happen:
  *      + either the call completes "normally", in which case we
  *        append call's timing info, and send to logger;
  *      + or another datatable call occurs once this call is pending
  *        (a nested call). In this case we send the `out_` message
  *        to logger immediately, set out_ to nullptr, and then when
  *        the call succeedes send '}' + timing info only.
  *
  * t_start_ is the time when the call started. This variable is
  *     initialized at the end of each `init_***()` call. Also, at
  *     the beginning of each call this variable is set to zero,
  *     which serves as an indicator that the call is being
  *     initialized. This is needed in order to avoid a situation
  *     like this: a call's header is being printed, which calls
  *     repr() on each argument, which in turn triggers a nested call
  *     into the datatable, which would flush `out_` and set it to
  *     nullptr. Then, as we continue to print the call's header, a
  *     crash occurs as we try to write into a dangling pointer.
  */
class CallLogger::Impl
{
  using stime_t = std::chrono::time_point<std::chrono::steady_clock>;
  private:
    const std::string             indent_;
    stime_t                       t_start_;
    std::unique_ptr<log::Message> out_;

  public:
    Impl(size_t i);
    void init_function  (const py::PKArgs* pkargs, py::robj args, py::robj kwds) noexcept;
    void init_method    (const py::PKArgs* pkargs, py::robj obj, py::robj args, py::robj kwds) noexcept;
    void init_dealloc   (py::robj obj) noexcept;
    void init_getset    (py::robj obj, py::robj val, void* closure) noexcept;
    void init_getsetitem(py::robj obj, py::robj key, py::robj val) noexcept;

    void emit_header() noexcept;
    void finish() noexcept;

  private:
    void print_arguments(py::robj pyargs, py::robj pykwds);
};



//---- Constructors ------------------------------------------------------------

CallLogger::Impl::Impl(size_t i)
    : indent_(2*i, ' ') {}


void CallLogger::Impl::init_function(
    const py::PKArgs* pkargs, py::robj args, py::robj kwds) noexcept
{
  try {
    t_start_ = stime_t();
    out_ = LOG->pinfo();
    *out_ << indent_;
    *out_ << "dt." << pkargs->get_short_name() << '(';
    print_arguments(args, kwds);
    *out_ << ')';
    t_start_ = std::chrono::steady_clock::now();
  } catch (...) {
    std::cerr << "... log failed\n";
  }
}


void CallLogger::Impl::init_method(
    const py::PKArgs* pkargs, py::robj obj, py::robj args, py::robj kwds)
    noexcept
{
  try {
    t_start_ = stime_t();
    out_ = LOG->pinfo();
    *out_ << indent_ << R(obj) << '.' << pkargs->get_short_name() << '(';
    print_arguments(args, kwds);
    *out_ << ')';
    t_start_ = std::chrono::steady_clock::now();
  } catch (...) {
    std::cerr << "... log failed\n";
  }
}


void CallLogger::Impl::init_dealloc(py::robj obj) noexcept {
  try {
    t_start_ = stime_t();
    out_ = LOG->pinfo();
    *out_ << indent_ << R(obj) << ".__del__()";
    t_start_ = std::chrono::steady_clock::now();
  } catch (...) {
    std::cerr << "... log failed\n";
  }
}


void CallLogger::Impl::init_getset(
    py::robj obj, py::robj val, void* closure) noexcept
{
  try {
    t_start_ = stime_t();
    const auto gsargs = static_cast<const py::GSArgs*>(closure);
    out_ = LOG->pinfo();
    *out_ << indent_ << R(obj) << '.' << gsargs->name;
    if (!val.is_undefined() && opt_report_args) {
      *out_ << " = " << R(val);
    }
    t_start_ = std::chrono::steady_clock::now();
  } catch (...) {
    std::cerr << "... log failed\n";
  }
}


void CallLogger::Impl::init_getsetitem(
    py::robj obj, py::robj key, py::robj val) noexcept
{
  try {
    t_start_ = stime_t();
    out_ = LOG->pinfo();
    *out_ << indent_ << R(obj) << '[';
    print_arguments(key, py::robj());
    *out_ << ']';
    if (!val.is_undefined() && opt_report_args) {
      *out_ << " = " << R(val);
    }
    t_start_ = std::chrono::steady_clock::now();
  } catch (...) {
    std::cerr << "... log failed\n";
  }
}




//---- Display -----------------------------------------------------------------

void CallLogger::Impl::emit_header() noexcept {
  if (!out_) return;                  // header already emitted
  if (t_start_ == stime_t()) return;  // still initializing, refuse to emit
  try {
    *out_ << " {";
    out_ = nullptr;   // the message is sent to Logger instance
  } catch (...) {
    std::cerr << "... log failed\n";  // LCOV_EXCL_LINE
  }
}


void CallLogger::Impl::finish() noexcept {
  try {
    stime_t t_end = std::chrono::steady_clock::now();
    std::chrono::duration<double> diff = t_end - t_start_;
    if (!out_) {
      out_ = LOG->pinfo();
      *out_ << indent_ << '}';
    }
    *out_ << " # "
          << (PyErr_Occurred()? "failed" : "done")
          << " in " << diff.count() << " s";
    out_ = nullptr;
  } catch (...) {
    std::cerr << "... log failed\n";  // LCOV_EXCL_LINE
  }
}


void CallLogger::Impl::print_arguments(py::robj args, py::robj kwds) {
  if (!opt_report_args) return;
  if (!args.is_undefined()) {
    if (args.is_tuple()) {
      auto arg_tuple = args.to_otuple();
      size_t n = arg_tuple.size();
      for (size_t i = 0; i < n; ++i) {
        if (i) *out_ << ", ";
        *out_ << R(arg_tuple[i]);
      }
    } else {
      *out_ << R(args);
    }
  }
  if (!kwds.is_undefined()) {
    if (!args.is_undefined()) *out_ << ", ";
    auto kwds_dict = kwds.to_rdict();
    bool print_comma = false;
    for (auto kv : kwds_dict) {
      if (print_comma) *out_ << ", ";
      *out_ << kv.first.to_cstring() << "=";
      *out_ << R(kv.second);
      print_comma = true;
    }
  }
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
  if (cl.impl_) {
    cl.impl_->init_function(pkargs, py::robj(pyargs), py::robj(pykwds));
  }
  return cl;
}


CallLogger CallLogger::method(const py::PKArgs* pkargs,
    PyObject* pythis, PyObject* pyargs, PyObject* pykwds) noexcept
{
  CallLogger cl;
  if (cl.impl_) {
    cl.impl_->init_method(pkargs, py::robj(pythis), py::robj(pyargs),
                          py::robj(pykwds));
  }
  return cl;
}


CallLogger CallLogger::dealloc(PyObject* pythis) noexcept {
  CallLogger cl;
  if (cl.impl_) cl.impl_->init_dealloc(pythis);
  return cl;
}


CallLogger CallLogger::getsetattr(PyObject* pythis, PyObject* val, void* closure) noexcept {
  CallLogger cl;
  if (cl.impl_) {
    cl.impl_->init_getset(py::robj(pythis), py::robj(val), closure);
  }
  return cl;
}


CallLogger CallLogger::getsetitem(PyObject* pythis, PyObject* key, PyObject* val) noexcept {
  CallLogger cl;
  if (cl.impl_) {
    cl.impl_->init_getsetitem(py::robj(pythis), py::robj(key), py::robj(val));
  }
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
