//------------------------------------------------------------------------------
// Copyright 2020-2021 H2O.ai
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
#include "expr/fexpr_func_unary.h"
#include "python/xargs.h"
#include "stype.h"
namespace dt {
namespace expr {


//------------------------------------------------------------------------------
// FExpr_AsType
//------------------------------------------------------------------------------

class FExpr_AsType : public FExpr_FuncUnary {
  private:
    dt::Type target_type_;

  public:
    FExpr_AsType(ptrExpr&& arg, dt::Type newtype)
      : FExpr_FuncUnary(std::move(arg)),
        target_type_(newtype)
    {}

    std::string name() const override {
      return "as_type";
    }

    std::string repr() const override {
      std::string out = FExpr_FuncUnary::repr();
      out.erase(out.size() - 1);  // remove ')'
      out += ", ";
      out += target_type_.to_string();
      out += ")";
      return out;
    }

    Column evaluate1(Column&& col) const override {
      col.cast_inplace(target_type_);
      return std::move(col);
    }
};




//------------------------------------------------------------------------------
// Python-facing `as_type()` function
//------------------------------------------------------------------------------

static py::oobj pyfn_astype(const py::XArgs& args) {
  auto cols = args[0].to_oobj();
  dt::Type newtype = args[1].to_type_force();
  return PyFExpr::make(new FExpr_AsType(as_fexpr(cols), newtype));
}

DECLARE_PYFN(&pyfn_astype)
    ->name("as_type")
    ->docs(doc_dt_as_type)
    ->arg_names({"cols", "new_type"})
    ->n_positional_args(2)
    ->n_required_args(2);




}}  // dt::expr
