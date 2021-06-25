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
#include "expr/re/fexpr_match.h"
#include "python/xargs.h"
namespace dt {
namespace expr {



std::string FExpr_Re_Match::name() const {
  return "match";
}

std::string FExpr_Re_Match::repr() const {
  std::string out = name();
  out += '(';
  out += arg_->repr();
  out += ')';
  return out;
}



//------------------------------------------------------------------------------
// Pyton-facing `re_match()` function
//------------------------------------------------------------------------------

static py::oobj fn_match(const py::XArgs& args) {
  (void) args;
  return py::None();
}

DECLARE_PYFN(&fn_match)
    ->name("re_match")
    ->n_required_args(2)
    ->n_positional_args(1)
    ->n_positional_or_keyword_args(1)
    ->arg_names({"column", "pattern"});




}}  // namespace dt::expr
