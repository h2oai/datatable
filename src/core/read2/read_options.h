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
#ifndef dt_READ2_READ_OPTIONS_h
#define dt_READ2_READ_OPTIONS_h
#include "read2/_declarations.h"
#include "utils/logger.h"
namespace dt {
namespace read2 {



class ReadOptions {
  private:
    dt::log::Logger logger_;
    NewlineKind     newlineKind_;
    QuoteKind       quoteKind_;
    QuoteRule       quoteRule_;
    SeparatorKind   separatorKind_;
    char            separatorChar_;
    bool            skipBlankLines_;
    bool            unevenRows_;
    int : 8;
    std::string     separatorString_;

  public:
    ReadOptions();
    ReadOptions(const ReadOptions&) = default;

    dt::log::Logger& logger();

    void initLogger(const py::Arg& logger, const py::Arg& verbose);
    void initSeparator(const py::Arg& separator);

    NewlineKind        getNewlineKind() const;
    QuoteKind          getQuoteKind() const;
    QuoteRule          getQuoteRule() const;
    SeparatorKind      getSeparatorKind() const;
    char               getSeparatorChar() const;
    const std::string& getSeparatorString() const;

    void setNewlineKind(NewlineKind);
    void setSeparatorKind(SeparatorKind);
    void setQuoteKind(QuoteKind);
    void setQuoteRule(QuoteRule);
    void setSeparatorChar(char);
    void setSeparatorString(const std::string&);
};




}}  // namespace dt::read2
#endif
