//------------------------------------------------------------------------------
// Copyright 2018 H2O.ai
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
#include <sstream>
#include "python/_all.h"
#include "python/obj.h"
#include "python/string.h"


struct ff {
  int width, precision;
  double value;
  ff(int w, int p, double v) : width(w), precision(p), value(v) {}
};


class LogMessage {
  private:
    std::ostringstream out;
    py::oobj logger;

  public:
    explicit LogMessage(py::oobj logger_) : logger(logger_) {}
    LogMessage(const LogMessage&) = delete;

    LogMessage(LogMessage&& other) {
      #if defined(__GNUC__) && __GNUC__ < 5
        // In gcc4.8 string stream was not moveable
        out << other.out.str();
      #else
        std::swap(out, other.out);
      #endif
      logger = std::move(other.logger);
    }

    ~LogMessage() {
      if (!logger) return;
      try {
        py::ostring s(out.str());
        logger.get_attr("debug").call({s});
      } catch (...) {}
    }

    template <typename T>
    LogMessage& operator <<(const T& value) {
      if (logger) out << value;
      return *this;
    }

    LogMessage& operator <<(const ff& f) {
      out << std::fixed << std::setw(f.width)
          << std::setprecision(f.precision)
          << f.value;
      return *this;
    }
};


#endif
