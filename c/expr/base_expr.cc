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
#include <memory>             // std::unique_ptr
#include "datatable.h"
#include "expr/base_expr.h"
#include "expr/py_expr.h"
#include "stdlib.h"


namespace dt {



//------------------------------------------------------------------------------
// workframe
//------------------------------------------------------------------------------

class workframe {
  private:
    std::vector<DataTable*> dts;
    std::vector<RowIndex> rowindexes;

  public:
    workframe() = default;
    workframe(const workframe&) = delete;
    workframe(workframe&&) = delete;

    DataTable* get_datatable(size_t id) const {
      return dts[id];
    }

    const RowIndex& get_rowindex(size_t id) const {
      return rowindexes[id];
    }
};



//------------------------------------------------------------------------------
// base_expr
//------------------------------------------------------------------------------

class base_expr {
  public:
    virtual ~base_expr();
    virtual SType resolve(const workframe&) = 0;
    virtual Column* evaluate_eager(const workframe&) = 0;
};

using base_expr_ptr = std::unique_ptr<base_expr>;

base_expr::~base_expr() {}



//------------------------------------------------------------------------------
// expr_column
//------------------------------------------------------------------------------


class expr_column : public base_expr {
  private:
    size_t frame_id;
    size_t col_id;
    py::oobj col_selector;

  public:
    expr_column(size_t dfid, const py::robj& col);
    SType resolve(const workframe&) override;
    Column* evaluate_eager(const workframe&) override;
};


expr_column::expr_column(size_t dfid, const py::robj& col)
  : frame_id(dfid), col_id(size_t(-1)), col_selector(col) {}


SType expr_column::resolve(const workframe& wf) {
  DataTable* dt = wf.get_datatable(frame_id);
  if (col_selector.is_int()) {
    int64_t icolid = col_selector.to_int64_strict();
    int64_t incols = static_cast<int64_t>(dt->ncols);
    if (icolid < -incols || icolid >= incols) {
      throw ValueError() << "Column index " << icolid << " is invalid for "
          "a Frame with " << incols << " column" << (incols == 1? "" : "s");
    }
    if (icolid < 0) icolid += incols;
    col_id = static_cast<size_t>(icolid);
  }
  else if (col_selector.is_string()) {
    col_id = dt->xcolindex(col_selector);
  }
  xassert(col_id < dt->ncols);
  return dt->columns[col_id]->stype();
}


Column* expr_column::evaluate_eager(const workframe& wf) {
  DataTable* dt = wf.get_datatable(frame_id);
  Column* rcol = dt->columns[col_id];
  const RowIndex& dt_ri = wf.get_rowindex(frame_id);
  const RowIndex& col_ri = rcol->rowindex();

  if (dt_ri && col_ri) {
    // TODO: implement rowindex cache in the workframe
    return rcol->shallowcopy(dt_ri * col_ri);
  }
  else if (dt_ri /* && !col_ri */) {
    return rcol->shallowcopy(dt_ri);
  }
  else {
    return rcol->shallowcopy();
  }
}



//------------------------------------------------------------------------------
// expr_binaryop
//------------------------------------------------------------------------------

static std::vector<std::string> binop_names;
static std::unordered_map<size_t, SType> binop_rules;

static size_t bin_id(binopCode opcode, SType st1, SType st2) {
  return (static_cast<size_t>(opcode) << 16) +
         (static_cast<size_t>(st1) << 8) +
         static_cast<size_t>(st2);
}

static void init_binops() {
  constexpr SType bool8 = SType::BOOL;
  std::vector<SType> integer_stypes = {
    SType::INT8, SType::INT16, SType::INT32, SType::INT64};
  std::vector<SType> numeric_stypes = {
    SType::BOOL, SType::INT8, SType::INT16, SType::INT32, SType::INT64,
    SType::FLOAT32, SType::FLOAT64};

  for (SType st1 : numeric_stypes) {
    for (SType st2 : numeric_stypes) {
      SType stm = std::max(st1, st2);
      binop_rules[bin_id(binopCode::PLUS, st1, st2)] = stm;
      binop_rules[bin_id(binopCode::MINUS, st1, st2)] = stm;
      binop_rules[bin_id(binopCode::MULTIPLY, st1, st2)] = stm;
      binop_rules[bin_id(binopCode::POWER, st1, st2)] = stm;
      binop_rules[bin_id(binopCode::DIVIDE, st1, st2)] = SType::FLOAT64;
      binop_rules[bin_id(binopCode::REL_EQ, st1, st2)] = bool8;
      binop_rules[bin_id(binopCode::REL_NE, st1, st2)] = bool8;
      binop_rules[bin_id(binopCode::REL_LT, st1, st2)] = bool8;
      binop_rules[bin_id(binopCode::REL_GT, st1, st2)] = bool8;
      binop_rules[bin_id(binopCode::REL_LE, st1, st2)] = bool8;
      binop_rules[bin_id(binopCode::REL_GE, st1, st2)] = bool8;
    }
  }
  for (SType st1 : integer_stypes) {
    for (SType st2 : integer_stypes) {
      SType stm = std::max(st1, st2);
      binop_rules[bin_id(binopCode::INT_DIVIDE, st1, st2)] = stm;
      binop_rules[bin_id(binopCode::MODULO, st1, st2)] = stm;
      binop_rules[bin_id(binopCode::LEFT_SHIFT, st1, st2)] = stm;
      binop_rules[bin_id(binopCode::RIGHT_SHIFT, st1, st2)] = stm;
    }
  }
  binop_rules[bin_id(binopCode::LOGICAL_AND, bool8, bool8)] = bool8;
  binop_rules[bin_id(binopCode::LOGICAL_OR, bool8, bool8)] = bool8;

  binop_names.resize(18);
  binop_names[size_t(binopCode::PLUS)] = "+";
  binop_names[size_t(binopCode::MINUS)] = "-";
}


class expr_binaryop : public base_expr {
  private:
    base_expr* lhs;
    base_expr* rhs;
    size_t binop_code;

  public:
    expr_binaryop(size_t opcode, base_expr* l, base_expr* r);
    SType resolve(const workframe& wf) override;
    Column* evaluate_eager(const workframe& wf) override;
};


expr_binaryop::expr_binaryop(size_t opcode, base_expr* l, base_expr* r)
  : lhs(l), rhs(r), binop_code(opcode) {}


SType expr_binaryop::resolve(const workframe& wf) {
  SType lhs_stype = lhs->resolve(wf);
  SType rhs_stype = rhs->resolve(wf);
  size_t triple = bin_id(static_cast<binopCode>(binop_code),
                         lhs_stype, rhs_stype);
  if (binop_rules.count(triple)) {
    throw TypeError() << "Binary operator `" << binop_names[binop_code]
        << "` cannot be applied to column with stypes `" << lhs_stype
        << "` and `" << rhs_stype << "`";
  }
  return binop_rules.at(triple);
}


Column* expr_binaryop::evaluate_eager(const workframe& wf) {
  Column* lhs_res = lhs->evaluate_eager(wf);
  Column* rhs_res = rhs->evaluate_eager(wf);
  return expr::binaryop(binop_code, lhs_res, rhs_res);
}



//------------------------------------------------------------------------------
// expr_literal
//------------------------------------------------------------------------------

class expr_literal : public base_expr {
  private:
    Column* col;

  public:
    explicit expr_literal(const py::robj&);
    ~expr_literal() override;
    SType resolve(const workframe&) override;
    Column* evaluate_eager(const workframe&) override;
};


expr_literal::expr_literal(const py::robj& v) {
  py::olist lst(1);
  lst.set(0, v);
  col = Column::from_pylist(lst, 0);
}

expr_literal::~expr_literal() {
  delete col;
}


SType expr_literal::resolve(const workframe&) {
  return col->stype();
}


Column* expr_literal::evaluate_eager(const workframe&) {
  return col->shallowcopy();
}




};


//------------------------------------------------------------------------------
// py::base_expr
//------------------------------------------------------------------------------

py::PKArgs py::base_expr::Type::args___init__(
    1, 0, 0, true, false, {"opcode"}, "__init__", nullptr);

const char* py::base_expr::Type::classname() {
  return "base_expr";
}

const char* py::base_expr::Type::classdoc() {
  return "Internal expression object";
}


static void check_args_count(const std::vector<py::robj>& va, size_t n) {
  if (va.size() == n) return;
  throw TypeError() << "Expected " << n << " additional arguments, but "
      "received " << va.size();
}

static dt::base_expr* to_base_expr(const py::robj& arg) {
  PyObject* v = arg.to_borrowed_ref();
  if (Py_TYPE(v) == &py::base_expr::Type::type) {
    auto vv = reinterpret_cast<py::base_expr*>(v);
    return vv->expr;
  }
  throw TypeError() << "Expected a base_expr object, but got " << arg.typeobj();
}


void py::base_expr::m__init__(py::PKArgs& args) {
  expr = nullptr;

  size_t opcode = args[0].to_size_t();
  std::vector<py::robj> va;
  va.reserve(args.num_vararg_args());
  for (auto item : args.varargs()) va.push_back(std::move(item));

  switch (opcode) {
    case dt::exprCode::COL: {
      check_args_count(va, 2);
      expr = new dt::expr_column(va[0].to_size_t(), va[1]);
      break;
    }
    case dt::exprCode::BINOP: {
      check_args_count(va, 3);
      size_t binop_code = va[0].to_size_t();
      dt::base_expr* lhs = to_base_expr(va[1]);
      dt::base_expr* rhs = to_base_expr(va[2]);
      expr = new dt::expr_binaryop(binop_code, lhs, rhs);
      break;
    }
    case dt::exprCode::LITERAL: {
      check_args_count(va, 1);
      expr = new dt::expr_literal(va[0]);
      break;
    }
  }
}

void py::base_expr::m__dealloc__() {
  delete expr;
}


void py::base_expr::Type::init_methods_and_getsets(
    py::ExtType<py::base_expr>::Methods&,
    py::ExtType<py::base_expr>::GetSetters&)
{
  dt::init_binops();
}
