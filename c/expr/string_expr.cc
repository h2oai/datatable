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
#include <regex>
#include "expr/base_expr.h"
#include "utils/exceptions.h"

namespace dt {


//------------------------------------------------------------------------------
// re_match()
//------------------------------------------------------------------------------

class expr_string_match_re : public base_expr {
  private:
    base_expr* arg;
    std::string pattern;
    std::regex regex;
    // int flags;

  public:
    expr_string_match_re(base_expr* expr, py::oobj params);
    SType resolve(const workframe& wf) override;
    GroupbyMode get_groupby_mode(const workframe&) const override;
    Column* evaluate_eager(workframe& wf) override;

  private:
    template <typename T>
    Column* _compute(Column* src);
};


expr_string_match_re::expr_string_match_re(base_expr* expr, py::oobj params) {
  arg = expr;
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
    throw ValueError() << "Invalid regular expression: " << e.what();
  }
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


Column* expr_string_match_re::evaluate_eager(workframe& wf) {
  Column* arg_res = arg->evaluate_eager(wf);
  SType arg_stype = arg_res->stype();
  xassert(arg_stype == SType::STR32 || arg_stype == SType::STR64);
  return arg_stype == SType::STR32? _compute<uint32_t>(arg_res)
                                  : _compute<uint64_t>(arg_res);
}


template <typename T>
Column* expr_string_match_re::_compute(Column* src) {
  auto ssrc = dynamic_cast<StringColumn<T>*>(src);
  size_t nrows = ssrc->nrows;
  RowIndex src_rowindex = ssrc->rowindex();
  const char* src_strdata = ssrc->strdata();
  const T* src_offsets = ssrc->offsets();

  auto trg = new BoolColumn(nrows);
  int8_t* trg_data = static_cast<int8_t*>(trg->data_w());

  #pragma omp parallel for schedule(dynamic)
  for (size_t i = 0; i < nrows; ++i) {
    size_t j = src_rowindex[i];
    T start = src_offsets[j - 1] & ~GETNA<T>();
    T end = src_offsets[j];
    if (ISNA<T>(end)) {
      trg_data[i] = GETNA<int8_t>();
      continue;
    }
    bool res = std::regex_match(src_strdata + start,
                                src_strdata + end,
                                regex);
    trg_data[i] = res;
  }
  return trg;
}




//------------------------------------------------------------------------------
// Factory function
//------------------------------------------------------------------------------

base_expr* expr_string_fn(size_t op, base_expr* arg, py::oobj params) {
  switch (static_cast<strop>(op)) {
    case strop::RE_MATCH: return new expr_string_match_re(arg, params);
  }
}



} // namespace dt
