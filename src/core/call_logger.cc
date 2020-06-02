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
#include "utils/logger.h"   // dt::log::Logger
namespace dt {

// LOG must be allocated dynamically, to prevent it from being
// destroyed at program exit after python runtime has already shut
// down (which will result in a crash, since Logger tries to DECREF
// a PyObject that it owns).
//
static log::Logger* LOG = nullptr;
static constexpr size_t N_IMPLS = 10;


//------------------------------------------------------------------------------
// Options
//------------------------------------------------------------------------------

static bool opt_report_args = false;

static void _init_options() {
  register_option(
    "debug.enabled",
    [] {
      return LOG? py::True() : py::False();
    },
    [](const py::Arg& arg) {
      bool value = arg.to_bool_strict();
      if (value) {    // debug.enabled = True
        if (!LOG) {
          LOG = new log::Logger;
          LOG->enable();
        }
      }
      else {          // debug.enabled = False
        if (LOG) {
          delete LOG;
          LOG = nullptr;
        }
      }
    },
    "If True, then calls to all C++ core functions will be timed\n"
    "and reported."
  );

  register_option(
    "debug.logger",
    [] {
      return LOG? LOG->get_pylogger(false)
                : py::None();
    },
    [](const py::Arg& arg) {
      if (arg.is_none()) {    // debug.logger = None
        if (LOG) {
          LOG->use_pylogger(py::None());
        }
      }
      else {                  // debug.logger = <object>
        if (!LOG) {
          LOG = new log::Logger;
        }
        LOG->use_pylogger(arg.to_oobj());
      }
    },
    "The logger object used for reporting calls to datatable core\n"
    "functions. If None, then messages will be sent directly to\n"
    "python stdout. Any object implementing the `.debug(msg)`\n"
    "method can be used as a logger.\n"
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
};


class CallLogger::Impl {
  using stime_t = std::chrono::time_point<std::chrono::steady_clock>;
  private:
    std::string       indent_;
    CallType          type_;
    const py::PKArgs* pkargs_;
    const PyObject*   pythis_;
    const PyObject*   pyargs_;
    const PyObject*   pykwds_;
    stime_t           t_start_;
    bool              header_printed_;
    size_t : 56;

  public:
    Impl(size_t i)
      : indent_(2*i, ' ') {}

    void init_function(const py::PKArgs* pkargs,
                       PyObject* pyargs, PyObject* pykwds) noexcept {
      type_ = CallType::FUNCTION;
      pkargs_ = pkargs;
      pyargs_ = pyargs;
      pykwds_ = pykwds;
      init_common();
    }

    void init_method(const py::PKArgs* pkargs, PyObject* pythis,
                     PyObject* pyargs, PyObject* pykwds) noexcept {
      type_ = CallType::METHOD;
      pkargs_ = pkargs;
      pythis_ = pythis;
      pyargs_ = pyargs;
      pykwds_ = pykwds;
      init_common();
    }

    void init_dealloc(PyObject* pythis) noexcept {
      type_ = CallType::DEALLOC;
      pythis_ = pythis;
      init_common();
    }

    void finish() noexcept {
      try {
        stime_t t_end = std::chrono::steady_clock::now();
        std::chrono::duration<double> diff = t_end - t_start_;
        auto msg = LOG->info();
        msg << indent_;
        if (header_printed_) {
          msg << '}';
        } else {
          print_name(msg);
          print_arguments(msg);
        }
        print_result(msg, diff.count());
        type_ = CallType::UNKNOWN;
      } catch (...) {
        std::cerr << "... log failed\n";  // LCOV_EXCL_LINE
      }
    }

  private:
    void init_common() noexcept {
      t_start_ = std::chrono::steady_clock::now();
      header_printed_ = false;
    }

    void print_name(log::Message& out) {
      switch (type_) {
        case CallType::FUNCTION: {
          out << "dt." << pkargs_->get_short_name();
          break;
        }
        case CallType::METHOD: {
          out << pkargs_->get_class_name() << '.' << pkargs_->get_short_name();
          break;
        }
        case CallType::DEALLOC: {
          const char* full_class_name =  pythis_->ob_type->tp_name;
          const char* class_name = std::strrchr(full_class_name, '.');
          if (class_name) class_name++;
          else            class_name = full_class_name;
          out << '~' << class_name;
          break;
        }
        default: {
          out << "<unknown>";
          break;
        }
      }
    }

    void print_arguments(log::Message& out) {
      (void)opt_report_args;
      out << "()";  // for now
    }

    void print_result(log::Message& out, double dtime) {
      out << " # "
          << (PyErr_Occurred()? "failed" : "done")
          << " in " << dtime << " s";
    }
};




//------------------------------------------------------------------------------
// CallLogger constructors
//------------------------------------------------------------------------------

std::vector<CallLogger::Impl*> CallLogger::impl_cache_;
size_t CallLogger::nested_level_ = 0;


CallLogger::CallLogger() noexcept {
  impl_ = nullptr;
  if (LOG) {
    if (nested_level_ < impl_cache_.size()) {
      impl_ = impl_cache_[nested_level_++];
    }
    else {
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


CallLogger::CallLogger(CallLogger&& other) {
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
