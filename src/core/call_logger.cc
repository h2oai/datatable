//------------------------------------------------------------------------------
// Copyright 2020-2021 H2O.ai
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
#include <algorithm>        // std::max
#include <chrono>
#include <iostream>
#include "call_logger.h"
#include "cstring.h"
#include "documentation.h"
#include "options.h"
#include "python/args.h"
#include "python/bool.h"
#include "python/string.h"
#include "python/xargs.h"
#include "utils/function.h"
#include "utils/logger.h"   // dt::log::Logger
namespace dt {

// These names should correspond to CallLogger::NbOp enum
static const char* nb_names[] = {
    "__add__",        // 0
    "__sub__",        // 1
    "__mul__",        // 2
    "__mod__",        // 3
    "__divmod__",     // 4
    "__pow__",        // 5
    "__lshift__",     // 6
    "__rshift__",     // 7
    "__and__",        // 8
    "__or__",         // 9
    "__xor__",        // 10
    "__truediv__",    // 11
    "__floordiv__",   // 12
    "__neg__",        // 13
    "__pos__",        // 14
    "__abs__",        // 15
    "__invert__",     // 16
    "__bool__",       // 17
    "__int__",        // 18
    "__float__",      // 19
    "__repr__",       // 20
    "__str__",        // 21
    "__iter__",       // 22
    "__next__",       // 23
};



// LOG must be allocated dynamically, to prevent it from being
// destroyed at program exit after python runtime has already shut
// down (which will result in a crash, since Logger tries to DECREF
// a PyObject that it owns).
//
static log::Logger* LOG = nullptr;


// This flag is equal to `LOG->enabled()` in most cases, except when
// we need to temporarily suspend logging (see Lock class).
//
static bool LOG_ENABLED = false;


// Number of `CallLogger::Impl`s in the cache. Normally we shouldn't
// see calls that are more than 2 levels deep, but who knows what
// the future will hold.
//
static constexpr size_t N_IMPLS = 10;



//------------------------------------------------------------------------------
// Options
//------------------------------------------------------------------------------

static bool opt_report_args = false;
static size_t opt_arg_max_size = 100;


static py::oobj get_enabled() {
  return py::obool(LOG_ENABLED);
}

static void set_enabled(const py::Arg& arg) {
  bool value = arg.to_bool_strict();
  if (value && !LOG_ENABLED) {
    LOG->enable();
    LOG_ENABLED = true;
  }
  if (!value && LOG_ENABLED) {
    LOG->disable();
    LOG_ENABLED = false;
  }
}


static py::oobj get_logger() {
  return LOG->get_pylogger(false);
}

static void set_logger(const py::Arg& arg) {
  auto logger = arg.to_oobj();
  if (logger.is_none()) {
    LOG->use_pylogger(py::oobj());
  }
  else {
    if (!logger.get_attrx("debug").is_callable()) {
      throw TypeError()
        << "Logger should be an object having a method .debug(self, msg)";
    }
    LOG->use_pylogger(logger);
  }
}


static py::oobj get_report_args() {
  return py::obool(opt_report_args);
}

static void set_report_args(const py::Arg& arg) {
  opt_report_args = arg.to_bool_strict();
}


static py::oobj get_arg_max_size() {
  return py::oint(opt_arg_max_size);
}

static void set_arg_max_size(const py::Arg& arg) {
  opt_arg_max_size = std::max(arg.to_size_t(), size_t(10));
}


static void _init_options() {
  register_option(
    "debug.enabled",
    get_enabled,
    set_enabled,
    doc_options_debug_enabled
  );

  register_option(
    "debug.logger",
    get_logger,
    set_logger,
    doc_options_debug_logger
  );

  register_option(
    "debug.report_args",
    get_report_args,
    set_report_args,
    doc_options_debug_report_args
  );

  register_option(
    "debug.arg_max_size",
    get_arg_max_size,
    set_arg_max_size,
    doc_options_debug_arg_max_size
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
  py::ostring repr = r.obj.safe_repr();
  auto strobj = repr.to_cstring();
  if (strobj.size() <= opt_arg_max_size) {
    auto len = static_cast<long>(strobj.size());
    out_.write(strobj.data(), len);
  } else {
    auto len0 = static_cast<long>(opt_arg_max_size * 3/5);
    auto len1 = static_cast<long>(opt_arg_max_size * 2/5 - 3);
    out_.write(strobj.data(), len0);
    out_.write("...", 3);
    out_.write(strobj.end() - len1, len1);
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
    void init_function  (const py::XArgs* xargs, py::robj args, py::robj kwds) noexcept;
    void init_method    (const py::PKArgs* pkargs, py::robj obj, py::robj args, py::robj kwds) noexcept;
    void init_method    (const py::XArgs* xargs, py::robj obj, py::robj args, py::robj kwds) noexcept;
    void init_dealloc   (py::robj obj) noexcept;
    void init_getset    (py::robj obj, py::robj val, void* closure) noexcept;
    void init_getattr   (py::robj obj, py::robj attr) noexcept;
    void init_getsetitem(py::robj obj, py::robj key, py::robj val) noexcept;
    void init_getbuffer (py::robj obj, void* buf, int flags) noexcept;
    void init_delbuffer (py::robj obj, void* buf) noexcept;
    void init_len       (py::robj obj) noexcept;
    void init_hash      (py::robj obj) noexcept;
    void init_unaryfn   (py::robj obj, int op) noexcept;
    void init_binaryfn  (py::robj x, py::robj y, int op) noexcept;
    void init_ternaryfn (py::robj x, py::robj y, py::robj z, int op) noexcept;
    void init_cmpfn     (py::robj x, py::robj y, int op) noexcept;

    void emit_header() noexcept;
    void finish() noexcept;

  private:
    void safe_init(dt::function<void()>) noexcept;
    void print_arguments(py::robj pyargs, py::robj pykwds);
};



//---- Constructors ------------------------------------------------------------

CallLogger::Impl::Impl(size_t i)
    : indent_(2*i, ' ') {}


void CallLogger::Impl::safe_init(dt::function<void()> init) noexcept {
  try {
    t_start_ = stime_t();
    out_ = LOG->pinfo();
    *out_ << indent_;
    init();
    t_start_ = std::chrono::steady_clock::now();
  } catch (...) {
    std::cerr << "... log failed\n";
  }
}


void CallLogger::Impl::init_function(
    const py::PKArgs* pkargs, py::robj args, py::robj kwds) noexcept
{
  safe_init([&] {
    *out_ << "dt." << pkargs->get_short_name() << '(';
    print_arguments(args, kwds);
    *out_ << ')';
  });
}


void CallLogger::Impl::init_function(
    const py::XArgs* xargs, py::robj args, py::robj kwds) noexcept
{
  safe_init([&] {
    *out_ << xargs->qualified_name() << '(';
    print_arguments(args, kwds);
    *out_ << ')';
  });
}


void CallLogger::Impl::init_method(
    const py::PKArgs* pkargs, py::robj obj, py::robj args, py::robj kwds)
    noexcept
{
  safe_init([&] {
    *out_ << R(obj) << '.' << pkargs->get_short_name() << '(';
    print_arguments(args, kwds);
    *out_ << ')';
  });
}


void CallLogger::Impl::init_method(
    const py::XArgs* xargs, py::robj obj, py::robj args, py::robj kwds)
    noexcept
{
  safe_init([&] {
    *out_ << R(obj) << '.' << xargs->qualified_name() << '(';
    print_arguments(args, kwds);
    *out_ << ')';
  });
}


void CallLogger::Impl::init_dealloc(py::robj obj) noexcept {
  safe_init([&] {
    *out_ << R(obj) << ".__del__()";
  });
}


void CallLogger::Impl::init_getset(
    py::robj obj, py::robj val, void* closure) noexcept
{
  const auto gsargs = static_cast<const py::GSArgs*>(closure);
  safe_init([&] {
    *out_ << R(obj) << '.' << gsargs->name;
    if (!val.is_undefined()) {
      *out_ << " =";
      if (opt_report_args) *out_ << ' ' << R(val);
    }
  });
}


void CallLogger::Impl::init_getattr(py::robj obj, py::robj attr) noexcept {
  safe_init([&] {
    *out_ << R(obj) << '.' << R(attr);
  });
}


void CallLogger::Impl::init_getsetitem(
    py::robj obj, py::robj key, py::robj val) noexcept
{
  safe_init([&] {
    *out_ << R(obj) << '[';
    print_arguments(key, py::robj());
    *out_ << ']';
    if (val.get() == CallLogger::GETITEM) {}
    else if (val.get() == CallLogger::DELITEM) { *out_ << " del"; }
    else {
      *out_ << " =";
      if (opt_report_args) *out_ << ' ' << R(val);
    }
  });
}


void CallLogger::Impl::init_getbuffer(
    py::robj obj, void* buf, int flags) noexcept
{
  safe_init([&] {
    *out_ << R(obj) << ".__getbuffer__(";
    if (opt_report_args) {
      *out_ << buf << ", " << flags;
    }
    *out_ << ')';
  });
}


void CallLogger::Impl::init_delbuffer(py::robj obj, void* buf) noexcept {
  safe_init([&] {
    *out_ << R(obj) << ".__releasebuffer__(";
    if (opt_report_args) {
      *out_ << buf;
    }
    *out_ << ')';
  });
}


void CallLogger::Impl::init_len(py::robj obj) noexcept {
  safe_init([&] {
    *out_ << R(obj) << ".__len__()";
  });
}


void CallLogger::Impl::init_hash(py::robj obj) noexcept {
  safe_init([&] {
    *out_ << R(obj) << ".__hash__()";
  });
}


void CallLogger::Impl::init_unaryfn(py::robj obj, int op) noexcept {
  safe_init([&] {
    *out_ << R(obj) << '.' << nb_names[op] << "()";
  });
}


void CallLogger::Impl::init_binaryfn(py::robj obj, py::robj other, int op) noexcept {
  safe_init([&] {
    *out_ << R(obj) << '.' << nb_names[op] << '(' << R(other) << ')';
  });
}


void CallLogger::Impl::init_ternaryfn(py::robj x, py::robj y, py::robj z, int op) noexcept {
  safe_init([&] {
    *out_ << R(x) << '.' << nb_names[op] << '(' << R(y) << ", " << R(z) << ')';
  });
}


void CallLogger::Impl::init_cmpfn(py::robj x, py::robj y, int op) noexcept {
  const char* name = (op == Py_LT)? "__lt__" :
                     (op == Py_LE)? "__le__" :
                     (op == Py_EQ)? "__eq__" :
                     (op == Py_NE)? "__ne__" :
                     (op == Py_GT)? "__gt__" :
                     (op == Py_GE)? "__ge__" : "__unknown__";
  safe_init([&] {
    *out_ << R(x) << '.' << name << '(' << R(y) << ')';
  });
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
    *out_ << " # " << diff.count() << " s";
    if (PyErr_Occurred()) *out_ << " (failed)";
    out_ = nullptr;   // the message is sent to Logger instance
  } catch (...) {
    std::cerr << "... log failed\n";  // LCOV_EXCL_LINE
  }
}


void CallLogger::Impl::print_arguments(py::robj args, py::robj kwds) {
  if (!opt_report_args) return;
  size_t n_args = 0;
  if (!args.is_undefined()) {
    if (args.is_tuple()) {
      auto arg_tuple = args.to_otuple();
      n_args = arg_tuple.size();
      for (size_t i = 0; i < n_args; ++i) {
        if (i) *out_ << ", ";
        *out_ << R(arg_tuple[i]);
      }
    } else {
      n_args = 1;
      *out_ << R(args);
    }
  }
  if (!kwds.is_undefined()) {
    if (n_args) *out_ << ", ";
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

PyObject* CallLogger::GETITEM = reinterpret_cast<PyObject*>(-1);
PyObject* CallLogger::DELITEM = reinterpret_cast<PyObject*>(0);


CallLogger::CallLogger() noexcept {
  impl_ = nullptr;
  if (LOG_ENABLED) {
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


CallLogger CallLogger::function(
    const py::XArgs* xargs, PyObject* pyargs, PyObject* pykwds) noexcept
{
  CallLogger cl;
  if (cl.impl_) {
    cl.impl_->init_function(xargs, py::robj(pyargs), py::robj(pykwds));
  }
  return cl;
}


CallLogger CallLogger::method(const py::PKArgs* pkargs,
    PyObject* pyobj, PyObject* pyargs, PyObject* pykwds) noexcept
{
  CallLogger cl;
  if (cl.impl_) {
    cl.impl_->init_method(pkargs, py::robj(pyobj), py::robj(pyargs),
                          py::robj(pykwds));
  }
  return cl;
}


CallLogger CallLogger::method(const py::XArgs* xargs,
    PyObject* pyobj, PyObject* pyargs, PyObject* pykwds) noexcept
{
  CallLogger cl;
  if (cl.impl_) {
    cl.impl_->init_method(xargs, py::robj(pyobj), py::robj(pyargs),
                          py::robj(pykwds));
  }
  return cl;
}


CallLogger CallLogger::dealloc(PyObject* pyobj) noexcept {
  CallLogger cl;
  if (cl.impl_) cl.impl_->init_dealloc(pyobj);
  return cl;
}


CallLogger CallLogger::getset(PyObject* pyobj, PyObject* val, void* closure) noexcept {
  CallLogger cl;
  if (cl.impl_) {
    cl.impl_->init_getset(py::robj(pyobj), py::robj(val), closure);
  }
  return cl;
}


CallLogger CallLogger::getattr(PyObject* pyobj, PyObject* attr) noexcept {
  CallLogger cl;
  if (cl.impl_) {
    cl.impl_->init_getattr(py::robj(pyobj), py::robj(attr));
  }
  return cl;
}


CallLogger CallLogger::getsetitem(PyObject* pyobj, PyObject* key, PyObject* val) noexcept {
  CallLogger cl;
  if (cl.impl_) {
    cl.impl_->init_getsetitem(py::robj(pyobj), py::robj(key), py::robj(val));
  }
  return cl;
}


CallLogger CallLogger::getbuffer(PyObject* pyobj, Py_buffer* buf, int flags) noexcept {
  CallLogger cl;
  if (cl.impl_) {
    cl.impl_->init_getbuffer(py::robj(pyobj), static_cast<void*>(buf), flags);
  }
  return cl;
}


CallLogger CallLogger::delbuffer(PyObject* pyobj, Py_buffer* buf) noexcept {
  CallLogger cl;
  if (cl.impl_) {
    cl.impl_->init_delbuffer(py::robj(pyobj), static_cast<void*>(buf));
  }
  return cl;
}


CallLogger CallLogger::len(PyObject* pyobj) noexcept {
  CallLogger cl;
  if (cl.impl_) {
    cl.impl_->init_len(py::robj(pyobj));
  }
  return cl;
}


CallLogger CallLogger::hash(PyObject* pyobj) noexcept {
  CallLogger cl;
  if (cl.impl_) {
    cl.impl_->init_hash(py::robj(pyobj));
  }
  return cl;
}


CallLogger CallLogger::unaryfn(PyObject* pyobj, int op) noexcept {
  CallLogger cl;
  if (cl.impl_) {
    cl.impl_->init_unaryfn(py::robj(pyobj), op);
  }
  return cl;
}


CallLogger CallLogger::binaryfn(PyObject* pyobj, PyObject* other, int op) noexcept {
  CallLogger cl;
  if (cl.impl_) {
    cl.impl_->init_binaryfn(py::robj(pyobj), py::robj(other), op);
  }
  return cl;
}


CallLogger CallLogger::ternaryfn(PyObject* x, PyObject* y, PyObject* z, int op) noexcept {
  CallLogger cl;
  if (cl.impl_) {
    cl.impl_->init_ternaryfn(py::robj(x), py::robj(y), py::robj(z), op);
  }
  return cl;
}


CallLogger CallLogger::cmpfn(PyObject* x, PyObject* y, int op) noexcept {
  CallLogger cl;
  if (cl.impl_) {
    cl.impl_->init_cmpfn(py::robj(x), py::robj(y), op);
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
  LOG = new log::Logger;
  xassert(!LOG->enabled());
  _init_options();
}




//------------------------------------------------------------------------------
// CallLoggerLock
//------------------------------------------------------------------------------

CallLoggerLock::CallLoggerLock() {
  enabled_previously_ = LOG_ENABLED;
  LOG_ENABLED = false;
}

CallLoggerLock::~CallLoggerLock() {
  LOG_ENABLED = enabled_previously_;
}




}  // namespace dt
