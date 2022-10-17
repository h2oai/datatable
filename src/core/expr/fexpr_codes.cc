//------------------------------------------------------------------------------
// Copyright 2022 H2O.ai
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
#include "_dt.h"
#include "documentation.h"
#include "column/const.h"
#include "column/sentinel_fw.h"
#include "expr/eval_context.h"
#include "expr/fexpr_func.h"
#include "python/xargs.h"
namespace dt {
namespace expr {


//------------------------------------------------------------------------------
// FExpr_Codes
//------------------------------------------------------------------------------

class FExpr_Codes : public FExpr_Func {
  private:
    ptrExpr arg_;

  public:
    FExpr_Codes(ptrExpr &&arg)
        : arg_(std::move(arg)) {}


    std::string repr() const override {
      std::string out = "codes";
      out += '(';
      out += arg_->repr();
      out += ')';
      return out;
    }


    Workframe evaluate_n(EvalContext &ctx) const override {
      Workframe wf = arg_->evaluate_n(ctx);

      for (size_t i = 0; i < wf.ncols(); ++i) {
        Column col = wf.retrieve_column(i);
        if (!col.type().is_categorical()) {
          throw TypeError() << "Invalid column of type `" << col.stype()
            << "` in " << repr();
        }

        Column col_codes;
        switch (col.stype()) {
          case SType::CAT8   : col_codes = evaluate1<int8_t>(col, SType::INT8); break;
          case SType::CAT16  : col_codes = evaluate1<int16_t>(col, SType::INT16); break;
          case SType::CAT32  : col_codes = evaluate1<int32_t>(col, SType::INT32); break;
          default: throw RuntimeError() << "Unknown categorical type: "
                     << col.stype();
        }

        wf.replace_column(i, std::move(col_codes));
      }
      return wf;
    }


    template <typename T>
    Column evaluate1(const Column& col, const SType stype) const {
      Column col_codes;
      if (col.n_children()) {
        col_codes = Column(new SentinelFw_ColumnImpl<T>(
                      col.nrows(),
                      stype,
                      Buffer(col.get_data_buffer(1))
                    ));
      } else {
        col_codes = Const_ColumnImpl::make_int_column(col.nrows(), 0, stype);
      }
      return col_codes;
    }
};



//------------------------------------------------------------------------------
// Python-facing `codes()` function
//------------------------------------------------------------------------------

static py::oobj pyfn_codes(const py::XArgs& args) {
  auto cols = args[0].to_oobj();
  return PyFExpr::make(new FExpr_Codes(as_fexpr(cols)));
}


DECLARE_PYFN(&pyfn_codes)
    ->name("codes")
    ->docs(doc_dt_codes)
    ->arg_names({"cols"})
    ->n_positional_args(1)
    ->n_required_args(1);


}}  // dt::expr
