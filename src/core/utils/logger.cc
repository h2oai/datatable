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
#include <ios>             // std::fixed
#include <iostream>
#include "call_logger.h"
#include "cstring.h"
#include "parallel/api.h"
#include "python/string.h"
#include "python/xobject.h"
#include "utils/assert.h"
#include "utils/logger.h"
#include "utils/terminal/terminal.h"
#include "utils/terminal/terminal_stream.h"


static void print_message(const std::string& message,
                          const std::string& prefix,
                          bool use_colors)
{
  dt::TerminalStream ts(use_colors);
  ts << dt::style::grey << prefix << message << dt::style::end << "\n";
  py::write_to_stdout(ts.str());
}



//------------------------------------------------------------------------------
// Python Logger class
//------------------------------------------------------------------------------
namespace py {


class DefaultLogger : public XObject<DefaultLogger>
{
  private:
    std::unique_ptr<std::string> prefix_;
    bool use_colors_;
    size_t : 56;

    void m__init__(const PKArgs&) {}

    void m__dealloc__() { prefix_ = nullptr; }

    void debug(const PKArgs& args) {
      auto msg = args[0].to_string();
      print_message(msg, *prefix_, use_colors_);
    }
    // oobj warning();

  public:
    static oobj make(const dt::log::Logger& logger) {
      oobj resobj = robj(DefaultLogger::typePtr).call();
      DefaultLogger* dlogger = DefaultLogger::cast_from(resobj);
      dlogger->prefix_ = std::make_unique<std::string>(logger.prefix_);
      dlogger->use_colors_ = logger.use_colors_;
      return resobj;
    }

    static void impl_init_type(XTypeMaker& xt) {
      xt.set_class_name("DefaultLogger");
      static PKArgs args_init(0, 0, 0, false, false, {}, "__init__", nullptr);
      static PKArgs args_debug(1, 0, 0, false, false, {"msg"}, "debug", nullptr);
      xt.add(CONSTRUCTOR(&DefaultLogger::m__init__, args_init));
      xt.add(DESTRUCTOR(&DefaultLogger::m__dealloc__));
      xt.add(METHOD(&DefaultLogger::debug, args_debug));
    }
};




}  // namespace py
namespace dt {
namespace log {



//------------------------------------------------------------------------------
// Section
//------------------------------------------------------------------------------

Section::Section(Logger* logger)
  : logger_(logger)
{
  xassert(logger);
}


Section::~Section() {
  logger_->end_section();
}




//------------------------------------------------------------------------------
// Message
//------------------------------------------------------------------------------

Message::Message(Logger* logger, bool warn)
  : logger_(logger), emit_as_warning_(warn)
{
  xassert(logger);
}


Message::~Message() {
  try {
    logger_->add(std::move(out_).str(), emit_as_warning_);
  }
  catch (...) {
    std::cerr << "unable to emit log message\n";
  }
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


plural::plural(size_t n, const std::string& s)
  : count_(n), str_(s.c_str()) {}

plural::plural(size_t n, const char* s)
  : count_(n), str_(s) {}

template <>
Message& Message::operator<<(const plural& pl) {
  out_ << pl.count_ << ' ';
  out_ << pl.str_;
  if (pl.count_ != 1) {  // see misc.py for more advanced pluralization rules
    out_ << 's';
  }
  return *this;
}



template <>
Message& Message::operator<<(const char& c) {
  uint8_t uc = static_cast<uint8_t>(c);
  if (uc < 0x20 || uc >= 0x80 || uc == '`' || uc == '\\') {
    out_ << '\\';
    if (c == '\n') out_ << 'n';
    else if (c == '\r') out_ << 'r';
    else if (c == '\t') out_ << 't';
    else if (c == '\\') out_ << '\\';
    else if (c == '`')  out_ << '`';
    else {
      uint8_t d1 = uc >> 4;
      uint8_t d2 = uc & 15;
      out_ << 'x' << static_cast<char>(d1 <= 9? '0' + d1 : d1 - 10 + 'a')
                  << static_cast<char>(d2 <= 9? '0' + d2 : d2 - 10 + 'a');
    }
  } else {
    out_ << c;
  }
  return *this;
}


template <>
Message& Message::operator<<(const dt::CString& str) {
  auto len = static_cast<long>(str.size());
  out_.write(str.data(), len);
  return *this;
}


template <>
Message& Message::operator<<(const py::robj& pyobj) {
  py::ostring repr = pyobj.repr();
  return *this << repr.to_cstring();
}

template <>
Message& Message::operator<<(const py::oobj& pyobj) {
  py::ostring repr = pyobj.repr();
  return *this << repr.to_cstring();
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

void Logger::disable() {
  enabled_ = false;
}

void Logger::use_pylogger(py::oobj logger) {
  pylogger_ = std::move(logger);
  enabled_ = true;
}


Section Logger::section(std::string title) {
  if (enabled_) {
    add(std::move(title), false);
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

std::unique_ptr<Message> Logger::pinfo() const {
  return std::make_unique<Message>(const_cast<Logger*>(this), false);
}



bool Logger::enabled() const {
  return enabled_;
}

py::oobj Logger::get_pylogger(bool fallback_to_default) const {
  if (enabled_) {
    if (pylogger_) {
      return pylogger_;
    }
    else if (fallback_to_default) {
      py::DefaultLogger::init_type();
      return py::DefaultLogger::make(*this);
    }
  }
  return py::None();
}



void Logger::end_section() noexcept {
  if (enabled_ && prefix_.size() >= 2) {
    // Presumably, string::resize() does not throw when the size is
    // being decreased.
    prefix_.resize(prefix_.size() - 2);
  }
}


// This function must be called from the main thread
void Logger::emit_pending_messages() {
  xassert(dt::this_thread_index() == 0);
  if (!pending_messages_.empty()) {
    for (size_t i = 0; i < pending_messages_.size(); i++) {
      auto msg = std::move(pending_messages_[i]);
      emit_(std::move(msg.first), msg.second);
    }
    pending_messages_.clear();
  }
}


void Logger::add(std::string&& msg, bool is_warning) {
  PythonLock pylock;
  CallLoggerLock loglock;
  if (dt::this_thread_index() == 0) {
    emit_pending_messages();
    emit_(std::move(msg), is_warning);
  } else {
    pending_messages_.push_back(std::make_pair(std::move(msg), is_warning));
  }
}


void Logger::emit_(std::string&& msg, bool warning) {
  // Use user-defined logger object
  if (pylogger_) {
    HidePythonError hpe;
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
      w.emit_warning();
    }
    else if (enabled_) {
      print_message(msg, prefix_, use_colors_);
    }
  }
}




}}  // namespace dt::log
