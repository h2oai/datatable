//------------------------------------------------------------------------------
// Copyright 2018-2021 H2O.ai
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
#include "read/field64.h"                // field64
#include "read/parse_context.h"          // ParseContext
#include "read/parsers/info.h"
namespace dt {
namespace read {


/**
  * "Void" type is a root for all other types. This type is matched by
  * empty column only, and there is nothing to read nor parsing
  * pointer to advance for an empty column.
  */
static void parse_void(const ParseContext&) {
}

REGISTER_PARSER(PT::Void)
    ->parser(parse_void)
    ->name("Empty")
    ->code('V')
    ->type(Type::void0())
    ->successors({PT::Bool01, PT::BoolU, PT::BoolT, PT::BoolL, PT::Int32,
                  PT::Int32Sep, PT::Int64, PT::Int64Sep, PT::Float32Hex,
                  PT::Float64Plain, PT::Float64Ext, PT::Float64Hex,
                  PT::Time64ISO, PT::Date32ISO, PT::Str32});




}}  // namespace dt::read::
