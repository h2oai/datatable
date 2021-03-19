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
#include "read/parsers/library.h"
namespace dt {
namespace read {

static constexpr int8_t NA_BOOL8 = -128;



//------------------------------------------------------------------------------
// Parse numbers 0 | 1 as boolean.
//------------------------------------------------------------------------------

static void parse_bool8_numeric(const ParseContext& ctx) {
  const char* ch = ctx.ch;
  if (ch < ctx.eof) {
    auto digit = static_cast<uint8_t>(*ch - '0');
    if (digit <= 1) {
      ctx.target->int8 = static_cast<int8_t>(digit);
      ctx.ch = ch + 1;
      return;
    }
  }
  ctx.target->int8 = NA_BOOL8;
}

REGISTER_PARSER(PT::Bool01)
    ->parser(parse_bool8_numeric)
    ->name("Bool8/numeric")
    ->code('b')
    ->type(Type::bool8())
    ->successors({PT::Int32, PT::Int32Sep, PT::Int64, PT::Int64Sep,
                  PT::Float64Plain, PT::Float64Ext, PT::Str32});



//------------------------------------------------------------------------------
// Parse lowercase true | false as boolean.
//------------------------------------------------------------------------------

static void parse_bool8_lowercase(const ParseContext& ctx) {
  const char* ch = ctx.ch;
  if (ch + 4 < ctx.eof && ch[0]=='f' && ch[1]=='a' && ch[2]=='l' && ch[3]=='s' && ch[4]=='e') {
    ctx.target->int8 = 0;
    ctx.ch = ch + 5;
  }
  else if (ch + 3 < ctx.eof && ch[0]=='t' && ch[1]=='r' && ch[2]=='u' && ch[3]=='e') {
    ctx.target->int8 = 1;
    ctx.ch = ch + 4;
  }
  else {
    ctx.target->int8 = NA_BOOL8;
  }
}

REGISTER_PARSER(PT::BoolL)
    ->parser(parse_bool8_lowercase)
    ->name("Bool8/lowercase")
    ->code('b')
    ->type(Type::bool8())
    ->successors({PT::Str32});




//------------------------------------------------------------------------------
// Parse titlecase True | False as boolean.
//------------------------------------------------------------------------------

static void parse_bool8_titlecase(const ParseContext& ctx) {
  const char* ch = ctx.ch;
  if (ch + 4 < ctx.eof && ch[0]=='F' && ch[1]=='a' && ch[2]=='l' && ch[3]=='s' && ch[4]=='e') {
    ctx.target->int8 = 0;
    ctx.ch = ch + 5;
  }
  else if (ch + 3 < ctx.eof && ch[0]=='T' && ch[1]=='r' && ch[2]=='u' && ch[3]=='e') {
    ctx.target->int8 = 1;
    ctx.ch = ch + 4;
  }
  else {
    ctx.target->int8 = NA_BOOL8;
  }
}

REGISTER_PARSER(PT::BoolT)
    ->parser(parse_bool8_titlecase)
    ->name("Bool8/titlecase")
    ->code('b')
    ->type(Type::bool8())
    ->successors({PT::Str32});




//------------------------------------------------------------------------------
// Parse uppercase TRUE | FALSE as boolean.
//------------------------------------------------------------------------------

static void parse_bool8_uppercase(const ParseContext& ctx) {
  const char* ch = ctx.ch;
  if (ch + 4 < ctx.eof && ch[0]=='F' && ch[1]=='A' && ch[2]=='L' && ch[3]=='S' && ch[4]=='E') {
    ctx.target->int8 = 0;
    ctx.ch = ch + 5;
  }
  else if (ch + 3 < ctx.eof && ch[0]=='T' && ch[1]=='R' && ch[2]=='U' && ch[3]=='E') {
    ctx.target->int8 = 1;
    ctx.ch = ch + 4;
  }
  else {
    ctx.target->int8 = NA_BOOL8;
  }
}

REGISTER_PARSER(PT::BoolU)
    ->parser(parse_bool8_uppercase)
    ->name("Bool8/uppercase")
    ->code('b')
    ->type(Type::bool8())
    ->successors({PT::Str32});




}} // namespace dt::read::
