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
#ifndef dt_READ_PARSERS_LIBRARY_h
#define dt_READ_PARSERS_LIBRARY_h
#include "_dt.h"
#include "read/parsers/pt.h"
#include "types/type.h"
#include "utils/macros.h"
namespace dt {
namespace read {

using ParserFnPtr = void(*)(const ParseContext&);
class ParserInfo2;


class ParserLibrary2 {
  public:
    static std::vector<ParserInfo2*>& all_parsers();
};



class ParserInfo2 {
  private:
    ParserFnPtr parser_;
    std::string name_;
    std::vector<PT> successors_;
    Type type_;
    PT id_;
    char code_;
    size_t : 48;

  public:
    ParserInfo2(PT p);

    //---- Property getters --------------------------------
    ParserFnPtr parser() const { return parser_; }
    const std::string& name() const { return name_; }
    const std::vector<PT>& successors() const { return successors_; }
    char code() const { return code_; }
    Type type() const { return type_; }

    //---- Property setters --------------------------------
    ParserInfo2* parser(ParserFnPtr p) {
      parser_ = p;
      return this;
    }

    ParserInfo2* name(std::string&& n) {
      name_ = std::move(n);
      return this;
    }

    ParserInfo2* code(char c) {
      code_ = c;
      return this;
    }

    ParserInfo2* type(Type t) {
      type_ = t;
      return this;
    }

    ParserInfo2* successors(std::vector<PT>&& sccsss) {
      successors_ = std::move(sccsss);
      return this;
    }
};


#define REGISTER_PARSER(ID) \
  static ParserInfo2* PASTE_TOKENS(parser_, __LINE__) = \
      (new ParserInfo2(ID))



}}  // namespace dt::read::
#endif
