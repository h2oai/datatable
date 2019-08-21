//------------------------------------------------------------------------------
// Copyright 2018 H2O.ai
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
#include "expr/expr.h"
#include "expr/expr_column.h"
#include "expr/expr_str.h"
#include "parallel/api.h"
#include "utils/exceptions.h"
#include "utils/macros.h"
#include "datatablemodule.h"
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
// re_match()
//------------------------------------------------------------------------------

expr_string_match_re::expr_string_match_re(pexpr&& expr, py::oobj params) {
  #if REGEX_SUPPORTED
    arg = std::move(expr);
    py::otuple tp = params.to_otuple();
    xassert(tp.size() == 2);

    // Pattern
    py::oobj pattern_arg = tp[0];
    if (pattern_arg.is_string()) {
      pattern = pattern_arg.to_string();
    } else if (pattern_arg.has_attr("pattern")) {
      pattern = pattern_arg.get_attr("pattern").to_string();
    } else {
      throw TypeError() << "Parameter `pattern` in .match_re() should be "
          "a string, instead got " << pattern_arg.typeobj();
    }

    // Flags (ignore for now)
    // py::oobj flags_arg = tp[1];

    try {
      regex = std::regex(pattern, std::regex::nosubs);
    } catch (const std::regex_error& e) {
      throw translate_exception(e);
    }
  #else
    (void) expr;
    (void) params;
    throw NotImplError() << "Regular expressions are not supported in your "
        "build of datatable (compiler: "
        << get_compiler_version_string() << ')';
  #endif
}


SType expr_string_match_re::resolve(const workframe& wf) {
  SType arg_stype = arg->resolve(wf);
  if (!(arg_stype == SType::STR32 || arg_stype == SType::STR64)) {
    throw TypeError() << "Method `.re_match()` cannot be applied to a "
        "column of type " << arg_stype;
  }
  return SType::BOOL;
}


GroupbyMode expr_string_match_re::get_groupby_mode(const workframe& wf) const {
  return arg->get_groupby_mode(wf);
}


OColumn expr_string_match_re::evaluate_eager(workframe& wf) {
  OColumn src = arg->evaluate_eager(wf);
  xassert(src.ltype() == LType::STRING);
  size_t nrows = src.nrows();

  OColumn trg = OColumn::new_data_column(SType::BOOL, nrows);
  int8_t* trg_data = static_cast<int8_t*>(trg->data_w());

  dt::parallel_for_dynamic(nrows,
    [&](size_t i) {
      CString value;
      bool isna = src.get_element(i, &value);
      if (isna) {
        trg_data[i] = GETNA<int8_t>();
        return;
      }
      bool res = std::regex_match(value.ch, value.ch + value.size, regex);
      trg_data[i] = res;
    });
  return trg;
}



}} // namespace dt::expr
