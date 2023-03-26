//------------------------------------------------------------------------------
// Copyright 2023 H2O.ai
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
#include "expr/funary/fexpr_isna.h"
#include "column/const.h"
#include "column/isna.h"
#include "documentation.h"
#include "expr/eval_context.h"
#include "expr/fexpr_column.h"
#include "expr/workframe.h"
#include "python/xargs.h"
namespace dt {
namespace expr {

FExpr_ISNA::FExpr_ISNA(ptrExpr &&arg) 
    : arg_(std::move(arg)) 
    {}

std::string FExpr_ISNA::repr() const {
  std::string out = "isna";
  out += '(';
  out += arg_->repr();
  out += ')';
  return out;
}

// static Column make_isna_col(Column &&col) {
//   switch (col.stype()) {
//   case SType::VOID:
//       return Const_ColumnImpl::make_bool_column(col.nrows(), true);
//   case SType::BOOL:
//   case SType::INT8:
//       return Column(new Isna_ColumnImpl<int8_t>(std::move(col)));
//   case SType::INT16:
//       return Column(new Isna_ColumnImpl<int16_t>(std::move(col)));
//   case SType::DATE32:
//   case SType::INT32:
//       return Column(new Isna_ColumnImpl<int32_t>(std::move(col)));
//   case SType::TIME64:
//   case SType::INT64:
//       return Column(new Isna_ColumnImpl<int64_t>(std::move(col)));
//   case SType::FLOAT32:
//       return Column(new Isna_ColumnImpl<float>(std::move(col)));
//   case SType::FLOAT64:
//       return Column(new Isna_ColumnImpl<double>(std::move(col)));
//   case SType::STR32:
//   case SType::STR64:
//       return Column(new Isna_ColumnImpl<CString>(std::move(col)));
//   default:
//       throw RuntimeError();
//   }
// }

Workframe FExpr_ISNA::evaluate_n(EvalContext &ctx) const {
  Workframe wf = arg_->evaluate_n(ctx);

  for (size_t i = 0; i < wf.ncols(); ++i) {
      Column coli = Column(new Isna_ColumnImpl(wf.retrieve_column(i)));
      wf.replace_column(i, std::move(coli));
  }

  return wf;
}

static py::oobj pyfn_isna(const py::XArgs &args) {
  auto isna = args[0].to_oobj();
  return PyFExpr::make(new FExpr_ISNA(as_fexpr(isna)));
}

DECLARE_PYFN(&pyfn_isna)
    ->name("isna")
    ->docs(doc_math_isna)
    ->arg_names({"cols"})
    ->n_positional_args(1)
    ->n_required_args(1);

} // namespace expr
} // namespace dt
