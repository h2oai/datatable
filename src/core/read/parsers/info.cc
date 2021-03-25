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
#include <iostream>
#include "read/parsers/info.h"
#include "utils/assert.h"
namespace dt {
namespace read {

ParserFnPtr* parser_functions = nullptr;
ParserInfo* parser_infos = nullptr;



//------------------------------------------------------------------------------
// PTInfoBuilder
//------------------------------------------------------------------------------

PTInfoBuilder::PTInfoBuilder(PT pt)
  : id_(pt)
{
  xassert(pt < PT::COUNT);
  get()->id_ = pt;
}


ParserInfo* PTInfoBuilder::get() {
  if (parser_infos == nullptr) {
    parser_infos = new ParserInfo[PT::COUNT];
    parser_functions = new ParserFnPtr[PT::COUNT];
  }
  return parser_infos + id_;
}

PTInfoBuilder* PTInfoBuilder::code(char c) {
  get()->code_ = c;
  return this;
}

PTInfoBuilder* PTInfoBuilder::name(std::string&& name) {
  get()->name_ = std::move(name);
  return this;
}

PTInfoBuilder* PTInfoBuilder::parser(ParserFnPtr fn) {
  get()->parser_ = fn;
  parser_functions[id_] = fn;
  return this;
}

PTInfoBuilder* PTInfoBuilder::successors(std::vector<PT>&& sc) {
  get()->successors_ = sc;
  return this;
}

PTInfoBuilder* PTInfoBuilder::type(Type type) {
  get()->type_ = type;
  return this;
}




}}
