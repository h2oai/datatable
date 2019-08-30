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

class head_literal_none : public head_literal {
  public:
    head_literal_none() {}

    Column make_column() const override {
      return Const_ColumnImpl::make_na_column(1);
    }
};




//------------------------------------------------------------------------------
// head_literal_bool
//------------------------------------------------------------------------------

class head_literal_bool : public head_literal {
  private:
    bool value;
    size_t : 56;

  public:
    head_literal_bool(bool x) : value(x) {}

    Column make_column() const override {
      return Const_ColumnImpl::make_bool_column(1, value);
    }
};




//------------------------------------------------------------------------------
// head_literal_int
//------------------------------------------------------------------------------

class head_literal_int : public head_literal {
  private:
    int64_t value;

  public:
    head_literal_int(int64_t x) : value(x) {}

    Column make_column() const override {
      return Const_ColumnImpl::make_int_column(1, value);
    }
};




//------------------------------------------------------------------------------
// head_literal_float
//------------------------------------------------------------------------------

class head_literal_float : public head_literal {
  private:
    double value;

  public:
    head_literal_float(double x) : value(x) {}

    Column make_column() const override {
      return Const_ColumnImpl::make_float_column(1, value);
    }
};




//------------------------------------------------------------------------------
// head_literal_string
//------------------------------------------------------------------------------

class head_literal_string : public head_literal {
  private:
    CString value;

  public:
    head_literal_string(CString x) : value(x) {}

    Column make_column() const override {
      return Const_ColumnImpl::make_string_column(1, value);
    }
};




//------------------------------------------------------------------------------
// head_literal
//------------------------------------------------------------------------------

ptrHead head_literal::from_none() {
  return ptrHead(new head_literal_none());
}

ptrHead head_literal::from_bool(bool x) {
  return ptrHead(new head_literal_bool(x));
}

ptrHead head_literal::from_int(int64_t x) {
  return ptrHead(new head_literal_int(x));
}

ptrHead head_literal::from_float(double x) {
  return ptrHead(new head_literal_float(x));
}

ptrHead head_literal::from_string(CString x) {
  return ptrHead(new head_literal_string(x));
}



Outputs head_literal::evaluate(const vecExpr& inputs, workframe&) const {
  (void) inputs;
  xassert(inputs.size() == 0);
  return Outputs().add(make_column(), Outputs::GroupToOne);
}



#pragma clang diagnostic pop

}}  // namespace dt::expr
