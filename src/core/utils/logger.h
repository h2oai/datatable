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
#include <sstream>    // std::ostringstream
#include <string>
#include <utility>
#include <vector>
#include "_dt.h"
namespace py { class DefaultLogger; }
namespace dt {
namespace log {

class Section;
class Message;
struct ff;


/**
  * Logger: a utility class which can be used to report some
  * debug-level information into python.
  *
  * This class may operate in one of the three modes:
  *   - "disabled" (`!enabled_`): messages are not written
  *     anywhere, and will get simply lost. This is the default;
  *   - "enabled" (`enabled_ && !pylogger_`): messages are written
  *     into python stdout, and colored grey (unless terminal settings
  *     prohibit colored text);
  *   - "python" (`enabled_ && pylogger_`): messages are sent to a
  *     python Logger object.
  *
  * When the object of this class is created, it is in the "disabled"
  * mode. Use `.enable()` to put into the "enabled" mode, and
  * `.use_pylogger()` to put into the "python" mode.
  *
  * Usage
  * -----
  *
  * logger.section(title)
  *   Emit `title` as a debug message, and return a log::Section
  *   object. While that object is alive, all subsequent writes to the
  *   logger will be indented. Multiple sections can be nested.
  *
  * logger.info()
  *   Returns a stream-like object which can be written to using the
  *   standard `<<` notation. When the object is destroyed, its
  *   contents will be submitted into the logger.
  *
  * logger.warn()
  *   Same as .info(), but the content of the object will be emitted
  *   as a warning.
  *
  */
class Logger {
  private:
    py::oobj pylogger_;
    std::string prefix_;
    std::vector<std::pair<std::string, bool>> pending_messages_;
    bool enabled_;
    bool use_colors_;
    size_t : 48;

  public:
    Logger();
    Logger(const Logger&) = default;
    Logger& operator=(const Logger&) = default;
    void enable();
    void disable();
    void use_pylogger(py::oobj);

    Section section(std::string title);
    Message info() const;
    Message warn() const;
    // Same as `info()`, but allocated on the heap
    std::unique_ptr<Message> pinfo() const;

    // Return true if the Logger is in "enabled" or "python" modes
    bool enabled() const;

    // "disabled" -> returns None
    // "enabled" -> returns an object of DefaultLogger class
    // "python" -> returns the stored python logger object
    py::oobj get_pylogger(bool fallback_to_default = true) const;

    void emit_pending_messages();

  private:
    void end_section() noexcept;
    void add(std::string&& msg, bool as_warning);
    void emit_(std::string&& msg, bool as_warning);

    friend class Section;
    friend class Message;
    friend class py::DefaultLogger;
};




/**
  * Helper object, returned by `Logger.section()`.
  */
class Section {
  private:
    Logger* logger_;

  public:
    Section(Logger*);
    Section(Section&&) = default;
    ~Section();
};




/**
  * Helper object, returned by `Logger.info()` and `Logger.warn()`.
  * This object supports stream-like interface.
  */
class Message {
  private:
    std::ostringstream out_;
    Logger* logger_;
    bool emit_as_warning_;
    size_t : 56;

  public:
    Message(Logger*, bool warn);
    Message(const Message&) = delete;
    Message(Message&&) = default;
    ~Message();

    template <typename T>
    Message& operator<<(const T& value) {
      out_ << value;
      return *this;
    }
};




/**
  * This struct can be used to write a formatted floating-point value.
  * The name "ff" stands for "fixed float". For example:
  *
  *     msg << ff(6, 3, val);  // equivalent to printf("%6.3f", val)
  *
  */
struct ff {
  int width, precision;
  double value;
  ff(int w, int h, double val);
};


/**
  */
struct plural {
  size_t count_;
  const char* str_;
  plural(size_t, const std::string&);
  plural(size_t, const char*);
};


template <> Message& Message::operator<<(const ff&);
template <> Message& Message::operator<<(const plural&);
template <> Message& Message::operator<<(const char&);
template <> Message& Message::operator<<(const CString&);
template <> Message& Message::operator<<(const py::robj&);
template <> Message& Message::operator<<(const py::oobj&);



}}  // namespace dt::log
#endif
