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
#include "column/func_unary.h"
#include "documentation.h"
#include "expr/str/fexpr_len.h"
#include "python/xargs.h"
namespace dt {
namespace expr {



//------------------------------------------------------------------------------
// FExpr_Str_Len
//------------------------------------------------------------------------------

static int64_t stringLength(const CString& str) {
  int64_t len = 0;
  auto ch = reinterpret_cast<const uint8_t*>(str.data());
  auto end = ch + str.size();
  while (ch < end) {
    uint8_t c = *ch;
    ch += (c < 0x80)? 1 :
          ((c & 0xE0) == 0xC0)? 2 :
          ((c & 0xF0) == 0xE0)? 3 :  4;
    len++;
  }
  return len;
}


std::string FExpr_Str_Len::name() const {
  return "str.len";
}


Column FExpr_Str_Len::evaluate1(Column&& col) const {
  auto nrows = col.nrows();
  if (!col.type().is_string()) {
    throw TypeError() << "Function `str.len()` cannot be applied to a "
        "column of type " << col.type();
  }
  return Column(new FuncUnary1_ColumnImpl<CString, int64_t>(
    std::move(col), stringLength, nrows, SType::INT64
  ));
}




//------------------------------------------------------------------------------
// Python-facing `str.len()` function
//------------------------------------------------------------------------------

static py::oobj fn_len(const py::XArgs& args) {
  auto arg_col = args[0].to_oobj();
  return PyFExpr::make(new FExpr_Str_Len(as_fexpr(arg_col)));
}

DECLARE_PYFN(&fn_len)
    ->name("str_len")
    ->docs(doc_str_len)
    ->n_required_args(1)
    ->n_positional_args(1)
    ->arg_names({"cols"});




}}  // namespace dt::expr
