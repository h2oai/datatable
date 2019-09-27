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
#include <unordered_map>
#include "column/const.h"
#include "expr/expr.h"
#include "expr/collist.h"
#include "expr/repl_node.h"
#include "expr/eval_context.h"
#include "utils/exceptions.h"
#include "column_impl.h"  // TODO: remove
#include "datatable.h"
#include "datatablemodule.h"
namespace dt {


//------------------------------------------------------------------------------
// frame_rn
//------------------------------------------------------------------------------

class frame_rn : public repl_node {
  DataTable* dtr;

  public:
    explicit frame_rn(DataTable* dt_) : dtr(dt_) {}
    void check_compatibility(size_t lrows, size_t lcols) const override;
    void replace_columns(EvalContext&, const intvec&) const override;
    void replace_values(EvalContext&, const intvec&) const override;
};


void frame_rn::check_compatibility(size_t lrows, size_t lcols) const {
  size_t rrows = dtr->nrows;
  size_t rcols = dtr->ncols;
  if ((rrows == lrows || rrows == 1) && (rcols == lcols || rcols == 1)) return;
  if (rcols == 0 && lcols == 0 && rrows == 0) return;
  throw ValueError() << "Invalid replacement Frame: expected [" <<
      lrows << " x " << lcols << "], but received [" << rrows <<
      " x " << rcols << "]";
}


void frame_rn::replace_columns(EvalContext& ctx, const intvec& indices) const {
  size_t rcols = dtr->ncols;
  size_t rrows = dtr->nrows;
  if (rcols == 0) return;

  DataTable* dt0 = ctx.get_datatable(0);
  size_t lcols = indices.size();
  size_t lrows = dt0->nrows;
  xassert(rcols == 1 || rcols == lcols);  // enforced in `check_compatibility()`

  Column col0;
  if (rcols == 1) {
    col0 = dtr->get_column(0);  // copy
    // Avoid resizing `col0` multiple times in the loop below
    if (rrows == 1) {
      col0.repeat(lrows);
    }
  }
  for (size_t i = 0; i < lcols; ++i) {
    size_t j = indices[i];
    Column coli = (rcols == 1)? col0 : dtr->get_column(i);  // copy
    if (coli.nrows() == 1) {
      coli.repeat(lrows);
    }
    dt0->set_ocolumn(j, std::move(coli));
  }
}


void frame_rn::replace_values(EvalContext& ctx, const intvec& indices) const {
  size_t rcols = dtr->ncols;
  size_t rrows = dtr->nrows;
  if (rcols == 0 || rrows == 0) return;

  DataTable* dt0 = ctx.get_datatable(0);
  const RowIndex& ri0 = ctx.get_rowindex(0);
  size_t lcols = indices.size();

  xassert(rcols == 1 || rcols == lcols);
  for (size_t i = 0; i < lcols; ++i) {
    size_t j = indices[i];
    const Column& coli = dtr->get_column(rcols == 1? 0 : i);
    if (!dt0->get_column(j)) {
      dt0->set_ocolumn(j,
          Column::new_na_column(coli.stype(), dt0->nrows));
    }
    Column& colj = dt0->get_column(j);
    colj.replace_values(ri0, coli);
  }
}




//------------------------------------------------------------------------------
// scalar_rn
//------------------------------------------------------------------------------
struct EnumClassHash {
  template <typename T>
  size_t operator()(T t) const noexcept { return static_cast<size_t>(t); }
};

class scalar_rn : public repl_node {
  public:
    void check_compatibility(size_t lrows, size_t lcols) const override;
    void replace_columns(EvalContext&, const intvec&) const override;
    void replace_values(EvalContext&, const intvec&) const override;

  protected:
    void check_column_types(const DataTable*, const intvec&) const;
    virtual const char* value_type() const noexcept = 0;
    virtual bool valid_ltype(LType lt) const noexcept = 0;
    virtual Column make_column(SType st, size_t nrows) const = 0;
};


void scalar_rn::check_compatibility(size_t, size_t) const {}


void scalar_rn::check_column_types(
  const DataTable* dt0, const intvec& indices) const
{
  for (size_t j : indices) {
    const Column& col = dt0->get_column(j);
    if (col && !valid_ltype(col.ltype())) {
      throw TypeError() << "Cannot assign " << value_type()
        << " value to column `" << dt0->get_names()[j]
        << "` of type " << col.stype();
    }
  }
}


void scalar_rn::replace_columns(EvalContext& ctx, const intvec& indices) const {
  DataTable* dt0 = ctx.get_datatable(0);
  check_column_types(dt0, indices);

  std::unordered_map<SType, Column, EnumClassHash> new_columns;
  for (size_t j : indices) {
    const Column& col = dt0->get_column(j);
    SType stype = col? col.stype() : SType::VOID;
    if (new_columns.count(stype) == 0) {
      new_columns[stype] = make_column(stype, dt0->nrows);
    }
    Column newcol = new_columns[stype];  // copy
    dt0->set_ocolumn(j, std::move(newcol));
  }
}


void scalar_rn::replace_values(EvalContext& ctx, const intvec& indices) const {
  DataTable* dt0 = ctx.get_datatable(0);
  const RowIndex& ri0 = ctx.get_rowindex(0);
  check_column_types(dt0, indices);

  for (size_t j : indices) {
    const Column& colj = dt0->get_column(j);
    SType stype = colj? colj.stype() : SType::VOID;
    Column replcol = make_column(stype, 1);
    stype = replcol.stype();  // may change from VOID to BOOL, FIXME!
    if (!colj) {
      dt0->set_ocolumn(j, Column::new_na_column(stype, dt0->nrows));
    }
    else if (colj.stype() != stype) {
      dt0->set_ocolumn(j, colj.cast(stype));
    }
    Column& ocol = dt0->get_column(j);
    ocol.replace_values(ri0, replcol);
  }
}




//------------------------------------------------------------------------------
// scalar_na_rn
//------------------------------------------------------------------------------

class scalar_na_rn : public scalar_rn {
  protected:
    const char* value_type() const noexcept override;
    bool valid_ltype(LType) const noexcept override;
    Column make_column(SType st, size_t nrows) const override;
};

const char* scalar_na_rn::value_type() const noexcept {
  return "None";  // LCOV_EXCL_LINE
}


bool scalar_na_rn::valid_ltype(LType) const noexcept {
  return true;
}


Column scalar_na_rn::make_column(SType st, size_t nrows) const {
  if (st == SType::VOID) st = SType::BOOL;
  return Column::new_na_column(st, nrows);
}




//------------------------------------------------------------------------------
// scalar_int_rn
//------------------------------------------------------------------------------

class scalar_int_rn : public scalar_rn {
  int64_t value;
  bool isbool;
  size_t : 56;

  public:
    explicit scalar_int_rn(int64_t x) : value(x), isbool(false) {}
    explicit scalar_int_rn(bool x)    : value(x), isbool(true) {}

  protected:
    const char* value_type() const noexcept override { return "integer"; }

    bool valid_ltype(LType lt) const noexcept override {
      return lt == LType::INT || lt == LType::REAL ||
             (lt == LType::BOOL && (value == 0 || value == 1));
    }

    Column make_column(SType st, size_t nrows) const override {
      return (isbool && (st == SType::VOID || st == SType::BOOL))
                ? Const_ColumnImpl::make_bool_column(nrows, bool(value))
                : Const_ColumnImpl::make_int_column(nrows, value, st);
    }
};




//------------------------------------------------------------------------------
// scalar_float_rn
//------------------------------------------------------------------------------

class scalar_float_rn : public scalar_rn {
  double value;

  public:
    explicit scalar_float_rn(double x) : value(x) {}

  protected:
    const char* value_type() const noexcept override;
    bool valid_ltype(LType) const noexcept override;
    Column make_column(SType st, size_t nrows) const override;
};


const char* scalar_float_rn::value_type() const noexcept {
  return "float";
}


bool scalar_float_rn::valid_ltype(LType lt) const noexcept {
  return lt == LType::REAL;
}


Column scalar_float_rn::make_column(SType st, size_t nrows) const {
  constexpr double MAX = double(std::numeric_limits<float>::max());
  // st can be either VOID or FLOAT32 or FLOAT64
  // VOID we always convert into FLOAT64 so as to avoid loss of precision;
  // otherwise we attempt to keep the old type `st`, unless doing so will lead
  // to value truncation (value does not fit into float32).
  bool res64 = (st == SType::FLOAT64 || st == SType::VOID ||
                std::abs(value) > MAX);
  SType result_stype = res64? SType::FLOAT64 : SType::FLOAT32;
  return Const_ColumnImpl::make_float_column(nrows, value, result_stype);
}




//------------------------------------------------------------------------------
// scalar_string_rn
//------------------------------------------------------------------------------

class scalar_string_rn : public scalar_rn {
  std::string value;

  public:
    explicit scalar_string_rn(std::string&& x) : value(std::move(x)) {}

  protected:
    const char* value_type() const noexcept override;
    bool valid_ltype(LType) const noexcept override;
    Column make_column(SType st, size_t nrows) const override;
};

const char* scalar_string_rn::value_type() const noexcept {
  return "string";
}

bool scalar_string_rn::valid_ltype(LType lt) const noexcept {
  return lt == LType::STRING;
}

Column scalar_string_rn::make_column(SType st, size_t nrows) const {
  if (st == SType::VOID) st = SType::STR32;
  if (nrows == 0) {
    return Column::new_data_column(SType::STR32, 0);
  }
  return Const_ColumnImpl::make_string_column(nrows, CString(value), st);
}




//------------------------------------------------------------------------------
// collist_rn
//------------------------------------------------------------------------------

class collist_rn : public repl_node {
  intvec indices;

  public:
    explicit collist_rn(collist* cl) : indices(cl->release_indices()) {}
    void check_compatibility(size_t lrows, size_t lcols) const override;
    void replace_columns(EvalContext&, const intvec&) const override;
    void replace_values(EvalContext&, const intvec&) const override;
};


void collist_rn::check_compatibility(size_t, size_t lcols) const {
  size_t rcols = indices.size();
  if (rcols == 1 || rcols == lcols) return;
  throw ValueError() << "Cannot replace "
      << lcols << " column" << (lcols==1? "s" : "") << " with"
      << rcols << " column" << (rcols==1? "s" : "");
}


void collist_rn::replace_columns(EvalContext&, const intvec&) const {
  throw NotImplError() << "collist_rn::replace_columns()";
}
void collist_rn::replace_values(EvalContext&, const intvec&) const {
  throw NotImplError() << "collist_rn::replace_values()";
}




//------------------------------------------------------------------------------
// exprlist_rn
//------------------------------------------------------------------------------

class exprlist_rn : public repl_node {
  exprvec exprs;

  public:
    explicit exprlist_rn(collist* cl) : exprs(cl->release_exprs()) {}
    void check_compatibility(size_t lrows, size_t lcols) const override;
    void replace_columns(EvalContext&, const intvec&) const override;
    void replace_values(EvalContext&, const intvec&) const override;
    void resolve(EvalContext&) const override;
};


void exprlist_rn::check_compatibility(size_t, size_t lcols) const {
  size_t rcols = exprs.size();
  if (rcols == 1 || rcols == lcols) return;
  throw ValueError() << "Cannot replace "
      << lcols << " column" << (lcols==1? "s" : "") << " with"
      << rcols << " column" << (rcols==1? "s" : "");
}


void exprlist_rn::resolve(EvalContext& ctx) const {
  for (auto& expr : exprs) {
    expr->resolve(ctx);
  }
}


void exprlist_rn::replace_columns(EvalContext& ctx, const intvec& indices) const {
  DataTable* dt0 = ctx.get_datatable(0);
  size_t lcols = indices.size();
  size_t rcols = exprs.size();
  xassert(lcols == rcols || rcols == 1);
  resolve(ctx);

  for (size_t i = 0; i < lcols; ++i) {
    size_t j = indices[i];
    Column col = (i < rcols)? exprs[i]->evaluate(ctx)
                            : dt0->get_column(indices[0]);
    xassert(col.nrows() == dt0->nrows);
    dt0->set_ocolumn(j, std::move(col));
  }
}


void exprlist_rn::replace_values(EvalContext&, const intvec&) const {
  throw NotImplError() << "exprlist_rn::replace_values()";
}




//------------------------------------------------------------------------------
// dt::repl_node
//------------------------------------------------------------------------------

repl_node::repl_node() {
  TRACK(this, sizeof(*this), "repl_node");
}

repl_node::~repl_node() {
  UNTRACK(this);
}


repl_node_ptr repl_node::make(EvalContext& ctx, py::oobj src) {
  repl_node* res = nullptr;

  if (src.is_frame())       res = new frame_rn(src.to_datatable());
  else if (src.is_none())   res = new scalar_na_rn();
  else if (src.is_bool())   res = new scalar_int_rn(bool(src.to_bool()));
  else if (src.is_int())    res = new scalar_int_rn(src.to_int64());
  else if (src.is_float())  res = new scalar_float_rn(src.to_double());
  else if (src.is_string()) res = new scalar_string_rn(src.to_string());
  else if (src.is_dtexpr() || src.is_list_or_tuple()) {
    collist cl(ctx, src, collist::REPL_NODE);
    if (cl.is_simple_list()) res = new collist_rn(&cl);
    else                     res = new exprlist_rn(&cl);
  }
  else {
    throw TypeError()
      << "The replacement value of unknown type " << src.typeobj();
  }
  return repl_node_ptr(res);
}


void repl_node::resolve(EvalContext&) const {}




}  // namespace dt
