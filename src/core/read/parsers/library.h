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
class PTInfoBuilder;


extern ParserFnPtr* parser_functions;
extern ParserInfo2* parser_infos;




class ParserInfo2 {
  friend class PTInfoBuilder;
  private:
    ParserFnPtr parser_;
    std::string name_;
    std::vector<PT> successors_;
    Type type_;
    PT id_;
    char code_;
    size_t : 48;

  public:
    ParserInfo2() = default;

    //---- Property getters ----------------------------------------------------
    ParserFnPtr parser() const                { return parser_; }
    const std::string& name() const           { return name_; }
    const std::vector<PT>& successors() const { return successors_; }
    char code() const                         { return code_; }
    Type type() const                         { return type_; }
};


/**
  * This is a helper class for REGISTER_PARSER macro.
  */
class PTInfoBuilder {
  private:
    PT id_;

  public:
    PTInfoBuilder(PT pt);

    PTInfoBuilder* code(char);
    PTInfoBuilder* name(std::string&&);
    PTInfoBuilder* parser(ParserFnPtr);
    PTInfoBuilder* successors(std::vector<PT>&&);
    PTInfoBuilder* type(Type);

  private:
    ParserInfo2* get();
};


// At the end of this macro, the static variable created here will contain
// a dangling pointer -- which is ok, since it won't be used in any way.
#define REGISTER_PARSER(ID) \
  static PTInfoBuilder* PASTE_TOKENS(ptbuilder_, __LINE__) = \
      std::make_unique<PTInfoBuilder>(ID)




}}  // namespace dt::read::
#endif
