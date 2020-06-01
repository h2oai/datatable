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

static log::Logger LOG;
static constexpr size_t N_IMPLS = 10;


//------------------------------------------------------------------------------
// Options
//------------------------------------------------------------------------------

static bool opt_report_args = false;

static void _init_options() {
  register_option(
    "debug.logger_enabled",
    [] {
      return py::obool(LOG.enabled());
    },
    [](const py::Arg& value) {
      bool enabled = value.to_bool_strict();
      if (enabled) LOG.enable();
      else         LOG.disable();
    },
    "If True, then calls to all C++ core functions will be timed\n"
    "and reported."
  );

  register_option(
    "debug.logger",
    [] {
      return LOG.get_pylogger(false);
    },
    [](const py::Arg& value) {
      LOG.use_pylogger(value.to_oobj_or_none());
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

class CallLogger::Impl {
  using stime_t = std::chrono::time_point<std::chrono::steady_clock>;
  private:
    std::string indent_;
    stime_t     t_start_;
    const char* name_;

  public:
    Impl(size_t i)
      : indent_(2*i, ' ') {}

    void init_from(const py::PKArgs& pkargs) noexcept {
      t_start_ = std::chrono::steady_clock::now();
      name_ = pkargs.get_long_name();
    }

    void finish() noexcept {
      try {
        stime_t t_end = std::chrono::steady_clock::now();
        std::chrono::duration<double> diff = t_end - t_start_;
        LOG.info() << indent_ << name_
                   << " # finished in " << diff.count() << " s";
      } catch (...) {
        std::cerr << indent_ << name_ << " # log failed\n";
      }
    }
};




//------------------------------------------------------------------------------
// CallLogger constructors
//------------------------------------------------------------------------------

std::vector<CallLogger::Impl*> CallLogger::impl_cache_;
size_t CallLogger::nested_level_ = 0;


CallLogger::CallLogger(const py::PKArgs& pkargs) noexcept {
  impl_ = nullptr;
  if (LOG.enabled()) {
    if (nested_level_ == impl_cache_.size()) {
      std::cerr << "nested call too deep\n";  // LCOV_EXCL_LINE
      return;                                 // LCOV_EXCL_LINE
    }
    impl_ = impl_cache_[nested_level_++];
    impl_->init_from(pkargs);
  }
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
