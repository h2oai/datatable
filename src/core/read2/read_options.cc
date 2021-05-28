//------------------------------------------------------------------------------
// Copyright 2021 H2O.ai
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
#include "read2/read_options.h"
#include "python/arg.h"
namespace dt {
namespace read2 {

#define DEBUG if (logger_.enabled()) logger_.info()


ReadOptions::ReadOptions()
  : separatorKind_(SeparatorKind::AUTO)
{}


dt::log::Logger& ReadOptions::logger() {
  return logger_;
}



void ReadOptions::initLogger(
    const py::Arg& argLogger, const py::Arg& argVerbose)
{
  bool verbose = argVerbose.to<bool>(false);
  if (argLogger.is_none_or_undefined()) {
    if (verbose) {
      logger_.enable();
    }
  } else {
    logger_.use_pylogger(argLogger.to_oobj());
  }
}


void ReadOptions::initSeparator(const py::Arg& argSep) {
  if (argSep.is_undefined()) {
    separatorKind_ = SeparatorKind::AUTO;
    return;
  }
  if (argSep.is_none()) {
    separatorKind_ = SeparatorKind::NONE;
  }
  else {
    // this will throw if argSep is not a string
    separatorString_ = argSep.to_string();
    if (separatorString_.size() == 0) {
      separatorKind_ = SeparatorKind::NONE;
    }
    else if (separatorString_.size() == 1) {
      separatorKind_ = SeparatorKind::CHAR;
      char c = separatorString_[0];
      separatorChar_ = c;
      if (c == '\n' || c == '\r') {
        separatorKind_ = SeparatorKind::NONE;
      }
      if (c=='"' || c=='\'' || c=='`' || ('0' <= c && c <= '9') ||
          ('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z')) {
        separatorKind_ = SeparatorKind::STRING;
      }
    }
    else {
      separatorKind_ = SeparatorKind::STRING;
      if (separatorString_ == "\\s+") {
        separatorKind_ = SeparatorKind::WHITESPACE;
      }
      if (separatorString_ == "auto") {
        separatorKind_ = SeparatorKind::AUTO;
      }
    }
  }
  if (!(separatorKind_ == SeparatorKind::CHAR ||
        separatorKind_ == SeparatorKind::STRING)) {
    separatorString_.clear();
  }
  if (logger_.enabled()) {
    switch (separatorKind_) {
      case SeparatorKind::AUTO: logger_.info() << "sep = <auto>"; break;
      case SeparatorKind::NONE: logger_.info() << "sep = <single-column mode>"; break;
      case SeparatorKind::CHAR: logger_.info() << "sep = <char>'" << separatorChar_ << "'"; break;
      case SeparatorKind::STRING: logger_.info() << "sep = <string>\"" << separatorString_ << "\""; break;
      case SeparatorKind::WHITESPACE: logger_.info() << "sep = <whitespace>"; break;
    }
  }
}


//------------------------------------------------------------------------------
// Parameters getters/setters
//------------------------------------------------------------------------------

NewlineKind ReadOptions::getNewlineKind() const {
  return newlineKind_;
}

QuoteKind ReadOptions::getQuoteKind() const {
  return quoteKind_;
}

QuoteRule ReadOptions::getQuoteRule() const {
  return quoteRule_;
}

SeparatorKind ReadOptions::getSeparatorKind() const {
  return separatorKind_;
}

char ReadOptions::getSeparatorChar() const {
  return separatorChar_;
}

const std::string& ReadOptions::getSeparatorString() const {
  return separatorString_;
}


void ReadOptions::setNewlineKind(NewlineKind nk) {
  newlineKind_ = nk;
}

void ReadOptions::setSeparatorKind(SeparatorKind sk) {
  separatorKind_ = sk;
}

void ReadOptions::setQuoteKind(QuoteKind qk) {
  quoteKind_ = qk;
}

void ReadOptions::setQuoteRule(QuoteRule qr) {
  quoteRule_ = qr;
}

void ReadOptions::setSeparatorChar(char c) {
  separatorChar_ = c;
}

void ReadOptions::setSeparatorString(const std::string& s) {
  separatorString_ = s;
}




}}  // namespace dt::read2
