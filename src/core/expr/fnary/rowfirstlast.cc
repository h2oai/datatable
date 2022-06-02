//------------------------------------------------------------------------------
// Copyright 2019-2022 H2O.ai
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
#include <algorithm>
#include "column/const.h"
#include "column/func_nary.h"
#include "documentation.h"
#include "expr/fnary/fnary.h"
#include "python/xargs.h"
namespace dt {
namespace expr {


template <bool FIRST>
std::string FExpr_RowFirstLast<FIRST>::name() const {
  return FIRST? "rowfirst" : "rowlast";
}



template <typename T, bool FIRST>
static bool op_rowfirstlast(size_t i, T* out, const colvec& columns) {
  size_t n = columns.size();
  for (size_t j = 0; j < n; ++j) {
    bool xvalid = columns[FIRST? j : n - j - 1].get_element(i, out);
    if (xvalid) return true;
  }
  return false;
}


template <typename T, bool FIRST>
static inline Column _rowfirstlast(colvec&& columns, SType outtype) {
  auto fn = op_rowfirstlast<T, FIRST>;
  size_t nrows = columns[0].nrows();
  return Column(new FuncNary_ColumnImpl<T>(
                    std::move(columns), fn, nrows, outtype));
}


template <bool FIRST>
Column FExpr_RowFirstLast<FIRST>::apply_function(colvec&& columns,
                                                 const size_t nrows,
                                                 const size_t) const
{
  if (columns.empty()) {
    return Const_ColumnImpl::make_na_column(nrows);
  }

  // Detect common stype
  SType stype0 = SType::VOID;
  for (const auto& col : columns) {
    stype0 = common_stype(stype0, col.stype());
  }
  if (stype0 == SType::INVALID) {
    throw TypeError() << "Incompatible column types in function `" << name() << "`";
  }
  promote_columns(columns, stype0);

  switch (stype0) {
    case SType::BOOL:    return _rowfirstlast<int8_t, FIRST>(std::move(columns), stype0);
    case SType::INT8:    return _rowfirstlast<int8_t, FIRST>(std::move(columns), stype0);
    case SType::INT16:   return _rowfirstlast<int16_t, FIRST>(std::move(columns), stype0);
    case SType::INT32:   return _rowfirstlast<int32_t, FIRST>(std::move(columns), stype0);
    case SType::INT64:   return _rowfirstlast<int64_t, FIRST>(std::move(columns), stype0);
    case SType::FLOAT32: return _rowfirstlast<float, FIRST>(std::move(columns), stype0);
    case SType::FLOAT64: return _rowfirstlast<double, FIRST>(std::move(columns), stype0);
    case SType::STR32:
    case SType::STR64:   return _rowfirstlast<CString, FIRST>(std::move(columns), stype0);
    default: {
      throw TypeError() << "Function `" << name() << "` doesn't support type `" << stype0 << "`";
    }
  }
}

template class FExpr_RowFirstLast<true>;
template class FExpr_RowFirstLast<false>;

DECLARE_PYFN(&py_rowfn)
    ->name("rowfirst")
    ->docs(doc_dt_rowfirst)
    ->allow_varargs()
    ->add_info(FN_ROWFIRST);

DECLARE_PYFN(&py_rowfn)
    ->name("rowlast")
    ->docs(doc_dt_rowlast)
    ->allow_varargs()
    ->add_info(FN_ROWLAST);




}}  // namespace dt::expr
