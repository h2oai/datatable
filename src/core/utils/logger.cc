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
#include "parallel/api.h"
#include "python/string.h"
#include "utils/logger.h"
#include "utils/terminal/terminal.h"
#include "utils/terminal/terminal_stream.h"
namespace dt {
namespace log {


//------------------------------------------------------------------------------
// Section
//------------------------------------------------------------------------------

Section::Section(Logger* logger)
  : logger_(logger) {}


Section::~Section() {
  logger_->end_section();
}




//------------------------------------------------------------------------------
// Message
//------------------------------------------------------------------------------

Message::Message(Logger* logger, bool warn)
  : logger_(logger), emit_as_warning_(warn) {}


Message::~Message() {
  logger_->emit(std::move(out_).str(), emit_as_warning_);
}



ff::ff(int w, int p, double v)
  : width(w), precision(p), value(v) {}

template <>
Message& Message::operator<<(const ff& f) {
  out_ << std::fixed << std::setw(f.width)
       << std::setprecision(f.precision)
       << f.value;
  return *this;
}




//------------------------------------------------------------------------------
// Logger
//------------------------------------------------------------------------------

Logger::Logger() {
  enabled_ = false;
  use_colors_ = dt::Terminal::standard_terminal().colors_enabled();
}


void Logger::enable() {
  enabled_ = true;
}

void Logger::use_pylogger(py::oobj logger) {
  pylogger_ = std::move(logger);
  enabled_ = true;
}


Section Logger::section(std::string title) {
  if (enabled_) {
    emit(std::move(title), false);
    prefix_ += "  ";
  }
  return Section(this);
}


Message Logger::info() const {
  return Message(const_cast<Logger*>(this), false);
}

Message Logger::warn() const {
  return Message(const_cast<Logger*>(this), true);
}


bool Logger::enabled() const {
  return enabled_;
}



void Logger::end_section() {
  if (enabled_) {
    prefix_.resize(prefix_.size() - 2);
  }
}


void Logger::emit(std::string&& msg, bool warning) {
  std::lock_guard<std::mutex> lock(dt::python_mutex());
  // Use user-defined logger object
  if (pylogger_) {
    if (warning) {
      pylogger_.invoke("warning", py::ostring(msg));
    } else {
      pylogger_.invoke("debug", py::ostring(prefix_ + msg));
    }
  }
  // Use default logging mechanism
  else {
    if (warning) {
      auto w = IOWarning();
      w << std::move(msg);
      w.emit();
    }
    else {
      if (!enabled_) return;
      if (use_colors_) {
        dt::TerminalStream ts(true);
        ts << dt::style::grey << prefix_ << msg << "\n" << dt::style::end;
        msg = ts.str();
      } else {
        msg = prefix_ + msg + "\n";
      }
      py::write_to_stdout(msg);
    }
  }
}




}}  // namespace dt::log
