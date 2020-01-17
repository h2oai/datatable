//------------------------------------------------------------------------------
// Copyright 2019 H2O.ai
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
#include <cstring>                 // std::memcmp
#include "column/virtual.h"
#include "expr/expr.h"
#include "expr/head_func_other.h"
#include "expr/workframe.h"
#include "utils/assert.h"
#include "utils/exceptions.h"
#include "utils/macros.h"
#include "datatablemodule.h"
namespace dt {
namespace expr {


//------------------------------------------------------------------------------
// Head_Func_Re_Match
//------------------------------------------------------------------------------

class re_match_vcol : public Virtual_ColumnImpl {
  private:
    Column arg;
    std::regex regex;

  public:
    re_match_vcol(Column&& col, const std::regex& rx)
      : Virtual_ColumnImpl(col.nrows(), SType::BOOL),
        arg(std::move(col)),
        regex(rx) {}

    ColumnImpl* clone() const override {
      return new re_match_vcol(Column(arg), regex);
    }

    bool get_element(size_t i, int8_t* out) const override {
      CString x;
      bool isvalid = arg.get_element(i, &x);
      if (isvalid) {
        *out = std::regex_match(x.ch, x.ch + x.size, regex);
      }
      return isvalid;
    }

    bool get_element(size_t i, int32_t* out) const override {
      CString x;
      bool isvalid = arg.get_element(i, &x);
      if (isvalid) {
        *out = std::regex_match(x.ch, x.ch + x.size, regex);
      }
      return isvalid;
    }
};


static Error translate_exception(const std::regex_error& e) {
  auto ret = ValueError() << "Invalid regular expression: ";
  const char* desc = e.what();
  if (std::memcmp(desc, "The expression ", 15) == 0) {
    ret << "it " << desc + 15;
  } else {
    ret << desc;
  }
  return ret;
}


ptrHead Head_Func_Re_Match::make(Op, const py::otuple& params) {
  xassert(params.size() == 2);
  return ptrHead(new Head_Func_Re_Match(params[0], params[1]));
}


Head_Func_Re_Match::Head_Func_Re_Match(py::robj arg_pattern, py::robj arg_flags) {
  (void) arg_flags;
  #if REGEX_SUPPORTED
    // Pattern
    if (arg_pattern.is_string()) {
      pattern = arg_pattern.to_string();
    }
    else if (arg_pattern.has_attr("pattern")) {
      pattern = arg_pattern.get_attr("pattern").to_string();
    }
    else {
      throw TypeError() << "Parameter `pattern` in .match_re() should be "
          "a string, instead got " << arg_pattern.typeobj();
    }

    try {
      regex = std::regex(pattern, std::regex::nosubs);
    } catch (const std::regex_error& e) {
      throw translate_exception(e);
    }
  #else
    (void) arg_pattern;
    throw NotImplError() << "Regular expressions are not supported in your "
        "build of datatable (compiler: "
        << get_compiler_version_string() << ')';
  #endif
}



Workframe Head_Func_Re_Match::evaluate_n(
    const vecExpr& args, EvalContext& ctx, bool) const
{
  xassert(args.size() == 1);
  Workframe outputs = args[0].evaluate_n(ctx);
  size_t n = outputs.ncols();
  for (size_t i = 0; i < n; ++i) {
    Column col = outputs.retrieve_column(i);
    if (col.ltype() != LType::STRING) {
      throw TypeError() << "Method `.re_match()` cannot be applied to a "
          "column of type " << col.stype();
    }
    outputs.replace_column(i, Column(new re_match_vcol(std::move(col), regex)));
  }
  return outputs;
}




}}  // namespace dt::expr
