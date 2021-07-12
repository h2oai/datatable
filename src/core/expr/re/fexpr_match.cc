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
#include "column/re_match.h"
#include "documentation.h"
#include "expr/re/fexpr_match.h"
#include "python/xargs.h"
#include "utils/exceptions.h"
namespace dt {
namespace expr {


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



//------------------------------------------------------------------------------
// FExpr_Re_Match
//------------------------------------------------------------------------------

FExpr_Re_Match::FExpr_Re_Match(ptrExpr&& arg, py::oobj pattern)
  : FExpr_FuncUnary(std::move(arg))
{
  if (pattern.is_string()) {
    pattern_ = pattern.to_string();
  }
  else if (pattern.has_attr("pattern")) {
    pattern_ = pattern.get_attr("pattern").to_string();
  }
  else {
    throw TypeError() << "Parameter `pattern` in re.match() should be "
        "a string, instead got " << pattern.typeobj();
  }

  try {
    regex_ = std::regex(pattern_, std::regex::nosubs);
  } catch (const std::regex_error& e) {
    throw translate_exception(e);
  }
}


std::string FExpr_Re_Match::name() const {
  return "re.match";
}

std::string FExpr_Re_Match::repr() const {
  std::string out = "re.match(";
  out += arg_->repr();
  out += ", r'";
  out += pattern_;
  out += "')";
  return out;
}


Column FExpr_Re_Match::evaluate1(Column&& col) const {
  return Column(new Re_Match_ColumnImpl(std::move(col), regex_));
}




//------------------------------------------------------------------------------
// Pyton-facing `re_match()` function
//------------------------------------------------------------------------------

static py::oobj fn_match(const py::XArgs& args) {
  auto arg_col = args[0].to_oobj();
  auto arg_pattern = args[1].to_oobj();
  return PyFExpr::make(new FExpr_Re_Match(as_fexpr(arg_col), arg_pattern));
}

DECLARE_PYFN(&fn_match)
    ->name("re_match")
    ->docs(doc_re_match)
    ->n_required_args(2)
    ->n_positional_args(1)
    ->n_positional_or_keyword_args(1)
    ->arg_names({"column", "pattern"});




}}  // namespace dt::expr
