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
#ifndef dt_UTILS_LOGGER_h
#define dt_UTILS_LOGGER_h
#include <iomanip>
#include "_dt.h"
namespace dt {
namespace log {

class LogSection;
class LogMessage;
struct ff;


class Logger {
  private:
    py::oobj pylogger_;
    std::string prefix_;
    bool enabled_;
    bool use_colors_;
    size_t : 48;

  public:
    Logger();
    Logger(const Logger&) = default;
    Logger& operator=(const Logger&) = default;
    void enable();
    void use_pylogger(py::oobj);

    LogSection section(std::string title);
    LogMessage info() const;
    LogMessage warn() const;
    bool enabled() const;

  private:
    void end_section();
    void emit(std::string&& msg, bool as_warning);

    friend class LogSection;
    friend class LogMessage;
};




class LogSection {
  private:
    Logger* logger_;

  public:
    LogSection(Logger*);
    LogSection(LogSection&&) = default;
    ~LogSection();
};




class LogMessage {
  private:
    std::ostringstream out_;
    Logger* logger_;
    bool emit_as_warning_;
    size_t : 56;

  public:
    LogMessage(Logger*, bool warn);
    LogMessage(const LogMessage&) = delete;
    LogMessage(LogMessage&&) = default;
    ~LogMessage();

    template <typename T>
    LogMessage& operator<<(const T& value) {
      out_ << value;
      return *this;
    }
};


struct ff {
  int width, precision;
  double value;
  ff(int w, int h, double val);
};

template <> LogMessage& LogMessage::operator<<(const ff&);




}}  // namespace dt::log
#endif
