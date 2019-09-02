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
#include "column/column_const.h"
#include "expr/head_literal.h"
#include "expr/expr.h"
#include "expr/outputs.h"
#include "expr/workframe.h"
#include "utils/assert.h"
#include "utils/exceptions.h"
namespace dt {
namespace expr {

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wweak-vtables"



//------------------------------------------------------------------------------
// head_literal_none
//------------------------------------------------------------------------------

class head_literal_none : public Head_Literal {
  public:
    Column eval_as_literal() const override {
      return Const_ColumnImpl::make_na_column(1);
    }

    Outputs eval_as_selector(workframe&, size_t) const override {
      throw RuntimeError();  // LCOV_EXCL_LINE
    }

    // When used as j, `None` means select all columns
    Outputs evaluate_j(const vecExpr&, workframe& wf) const override {
      auto dt0 = wf.get_datatable(0);
      Outputs res;
      for (size_t i = 0; i < dt0->ncols; ++i) {
        res.add_column(dt0, i);
      }
      return res;
    }

    // When used in f, `None` means select nothing
    Outputs evaluate_f(const vecExpr&, workframe&, size_t) const override {
      return Outputs();
    }
};




//------------------------------------------------------------------------------
// head_literal_bool
//------------------------------------------------------------------------------

class head_literal_bool : public Head_Literal {
  private:
    bool value;
    size_t : 56;

  public:
    head_literal_bool(bool x) : value(x) {}

    Column eval_as_literal() const override {
      return Const_ColumnImpl::make_bool_column(1, value);
    }

    Outputs eval_as_selector(workframe&, size_t) const override {
      throw TypeError()
        << "A boolean value cannot be used as a column selector";
    }
};




//------------------------------------------------------------------------------
// head_literal_int
//------------------------------------------------------------------------------

class head_literal_int : public Head_Literal {
  private:
    int64_t value;

  public:
    head_literal_int(int64_t x) : value(x) {}

    Column eval_as_literal() const override {
      return Const_ColumnImpl::make_int_column(1, value);
    }

    Outputs eval_as_selector(workframe& wf, size_t frame_id) const override {
      auto df = wf.get_datatable(frame_id);
      int64_t icols = static_cast<int64_t>(df->ncols);
      if (value < -icols || value >= icols) {
        throw ValueError()
            << "Column index `" << value << "` is invalid for a Frame with "
            << icols << " column" << (icols == 1? "" : "s");
      }
      size_t i = static_cast<size_t>(value < 0? value + icols : value);
      return Outputs().add(Column(df->get_column(i)),
                           std::string(df->get_names()[i]),
                           Outputs::GroupToAll);
    }
};




//------------------------------------------------------------------------------
// head_literal_float
//------------------------------------------------------------------------------

class head_literal_float : public Head_Literal {
  private:
    double value;

  public:
    head_literal_float(double x) : value(x) {}

    Column eval_as_literal() const override {
      return Const_ColumnImpl::make_float_column(1, value);
    }

    Outputs eval_as_selector(workframe&, size_t) const override {
      throw TypeError() << "A floating-point value cannot be used as a "
          "column selector";
    }
};




//------------------------------------------------------------------------------
// head_literal_string
//------------------------------------------------------------------------------

class head_literal_string : public Head_Literal {
  private:
    py::oobj pystr;

  public:
    head_literal_string(py::robj x)
      : pystr(x) {}

    Column eval_as_literal() const override {
      return Const_ColumnImpl::make_string_column(1, pystr.to_string());
    }

    Outputs eval_as_selector(workframe& wf, size_t frame_id) const override {
      auto df = wf.get_datatable(frame_id);
      size_t j = df->xcolindex(pystr);
      return Outputs().add_column(df, j);
    }
};




//------------------------------------------------------------------------------
// Head_Literal
//------------------------------------------------------------------------------

ptrHead Head_Literal::from_none() {
  return ptrHead(new head_literal_none());
}

ptrHead Head_Literal::from_bool(bool x) {
  return ptrHead(new head_literal_bool(x));
}

ptrHead Head_Literal::from_int(int64_t x) {
  return ptrHead(new head_literal_int(x));
}

ptrHead Head_Literal::from_float(double x) {
  return ptrHead(new head_literal_float(x));
}

ptrHead Head_Literal::from_string(py::robj x) {
  return ptrHead(new head_literal_string(x));
}



Outputs Head_Literal::evaluate(const vecExpr& inputs, workframe&) const {
  (void) inputs;
  xassert(inputs.size() == 0);
  return Outputs().add(eval_as_literal(), Outputs::GroupToOne);
}



Outputs Head_Literal::evaluate_j(const vecExpr& inputs, workframe& wf) const {
  (void) inputs;
  xassert(inputs.size() == 0);
  return eval_as_selector(wf, 0);
}



Outputs Head_Literal::evaluate_f(
    const vecExpr& inputs, workframe& wf, size_t frame_id) const
{
  (void) inputs;
  xassert(inputs.size() == 0);
  return eval_as_selector(wf, frame_id);
}




#pragma clang diagnostic pop

}}  // namespace dt::expr
