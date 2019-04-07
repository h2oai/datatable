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
#include "expr/base_expr.h"
#include "expr/collist.h"
#include "expr/repl_node.h"
#include "expr/workframe.h"
#include "utils/exceptions.h"
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
    void replace_columns(workframe&, const intvec&) const override;
    void replace_values(workframe&, const intvec&) const override;
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


void frame_rn::replace_columns(workframe& wf, const intvec& indices) const {
  size_t rcols = dtr->ncols;
  size_t rrows = dtr->nrows;
  if (rcols == 0) return;

  DataTable* dt0 = wf.get_datatable(0);
  size_t lcols = indices.size();
  size_t lrows = dt0->nrows;
  xassert(rcols == 1 || rcols == lcols);  // enforced in `check_compatibility()`

  Column* col0 = nullptr;
  if (rcols == 1) {
    col0 = dtr->columns[0]->shallowcopy();
    // Avoid resizing `col0` multiple times in the loop below
    if (rrows == 1) {
      col0->resize_and_fill(lrows);  // TODO: use function from repeat.cc
    }
  }
  for (size_t i = 0; i < lcols; ++i) {
    size_t j = indices[i];
    Column* coli = rcols == 1? col0->shallowcopy()
                             : dtr->columns[i]->shallowcopy();
    if (coli->nrows == 1) {
      coli->resize_and_fill(lrows);  // TODO: use function from repeat.cc
    }
    delete dt0->columns[j];
    dt0->columns[j] = coli;
  }
  delete col0;
}


void frame_rn::replace_values(workframe& wf, const intvec& indices) const {
  size_t rcols = dtr->ncols;
  size_t rrows = dtr->nrows;
  if (rcols == 0 || rrows == 0) return;

  DataTable* dt0 = wf.get_datatable(0);
  const RowIndex& ri0 = wf.get_rowindex(0);
  size_t lcols = indices.size();

  xassert(rcols == 1 || rcols == lcols);
  for (size_t i = 0; i < lcols; ++i) {
    size_t j = indices[i];
    Column* coli = dtr->columns[rcols == 1? 0 : i];
    dt0->columns[j]->replace_values(ri0, coli);
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
    void replace_columns(workframe&, const intvec&) const override;
    void replace_values(workframe&, const intvec&) const override;

  protected:
    void check_column_types(const DataTable*, const intvec&) const;
    virtual const char* value_type() const noexcept = 0;
    virtual bool valid_ltype(LType lt) const noexcept = 0;
    virtual colptr make_column(SType st, size_t nrows) const = 0;
};


void scalar_rn::check_compatibility(size_t, size_t) const {}


void scalar_rn::check_column_types(
  const DataTable* dt0, const intvec& indices) const
{
  for (size_t j : indices) {
    Column* col = dt0->columns[j];
    if (col && !valid_ltype(col->ltype())) {
      throw TypeError() << "Cannot assign " << value_type()
        << " value to column `" << dt0->get_names()[j]
        << "` of type " << col->stype();
    }
  }
}


void scalar_rn::replace_columns(workframe& wf, const intvec& indices) const {
  DataTable* dt0 = wf.get_datatable(0);
  check_column_types(dt0, indices);

  std::unordered_map<SType, colptr, EnumClassHash> new_columns;
  for (size_t j : indices) {
    Column* col = dt0->columns[j];
    SType st = col? col->stype() : SType::VOID;
    if (new_columns.count(st) == 0) {
      new_columns[st] = make_column(st, dt0->nrows);
    }
    delete col;
    dt0->columns[j] = new_columns[st]->shallowcopy();
  }
}


void scalar_rn::replace_values(workframe& wf, const intvec& indices) const {
  DataTable* dt0 = wf.get_datatable(0);
  const RowIndex& ri0 = wf.get_rowindex(0);
  check_column_types(dt0, indices);

  for (size_t j : indices) {
    Column* col = dt0->columns[j];
    SType st = col? col->stype() : SType::VOID;
    colptr replcol = make_column(st, 1);
    if (col) {
      SType res_stype = replcol->stype();
      if (col->stype() != res_stype) {
        dt0->columns[j] = col->cast(res_stype);
        delete col;
        col = dt0->columns[j];
      }
    } else {
      col = Column::new_na_column(replcol->stype(), dt0->nrows);
      dt0->columns[j] = col;
    }
    col->replace_values(ri0, replcol.get());
  }
}




//------------------------------------------------------------------------------
// scalar_na_rn
//------------------------------------------------------------------------------

class scalar_na_rn : public scalar_rn {
  protected:
    const char* value_type() const noexcept override;
    bool valid_ltype(LType) const noexcept override;
    colptr make_column(SType st, size_t nrows) const override;
};

const char* scalar_na_rn::value_type() const noexcept {
  return "None";  // LCOV_EXCL_LINE
}


bool scalar_na_rn::valid_ltype(LType) const noexcept {
  return true;
}


colptr scalar_na_rn::make_column(SType st, size_t nrows) const {
  if (st == SType::VOID) st = SType::BOOL;
  return colptr(Column::new_na_column(st, nrows));
}




//------------------------------------------------------------------------------
// scalar_int_rn
//------------------------------------------------------------------------------

class scalar_int_rn : public scalar_rn {
  int64_t value;

  public:
    explicit scalar_int_rn(int64_t x) : value(x) {}

  protected:
    const char* value_type() const noexcept override;
    bool valid_ltype(LType) const noexcept override;
    colptr make_column(SType st, size_t nrows) const override;
    template <typename T> colptr _make1(SType st) const;
};


const char* scalar_int_rn::value_type() const noexcept {
  return "integer";
}


bool scalar_int_rn::valid_ltype(LType lt) const noexcept {
  return lt == LType::INT || lt == LType::REAL ||
         (lt == LType::BOOL && (value == 0 || value == 1));
}


colptr scalar_int_rn::make_column(SType st, size_t nrows) const {
  int64_t av = std::abs(value);
  SType rst = value == 0 || value == 1? SType::BOOL :
              av <= 127? SType::INT8 :
              av <= 32767? SType::INT16 :
              av <= 2147483647? SType::INT32 : SType::INT64;
  if (static_cast<size_t>(st) > static_cast<size_t>(rst)) {
    rst = st;
  }
  colptr col1 = rst == SType::BOOL? _make1<int8_t>(rst) :
                rst == SType::INT8? _make1<int8_t>(rst) :
                rst == SType::INT16? _make1<int16_t>(rst) :
                rst == SType::INT32? _make1<int32_t>(rst) :
                rst == SType::INT64? _make1<int64_t>(rst) :
                rst == SType::FLOAT32? _make1<float>(rst) :
                rst == SType::FLOAT64? _make1<double>(rst) : nullptr;
  xassert(col1);
  return colptr(col1->repeat(nrows));
}


template <typename T>
colptr scalar_int_rn::_make1(SType st) const {
  Column* col = Column::new_data_column(st, 1);
  auto tcol = static_cast<FwColumn<T>*>(col);
  tcol->set_elem(0, static_cast<T>(value));
  return colptr(col);
}




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
    colptr make_column(SType st, size_t nrows) const override;
};


const char* scalar_float_rn::value_type() const noexcept {
  return "float";
}


bool scalar_float_rn::valid_ltype(LType lt) const noexcept {
  return lt == LType::REAL;
}


colptr scalar_float_rn::make_column(SType st, size_t nrows) const {
  constexpr double MAX = double(std::numeric_limits<float>::max());
  // st can be either VOID or FLOAT32 or FLOAT64
  // VOID we always convert into FLOAT64 so as to avoid loss of precision;
  // otherwise we attempt to keep the old type `st`, unless doing so will lead
  // to value truncation (value does not fit into float32).
  SType rst = st == SType::VOID || std::abs(value) > MAX
              ? SType::FLOAT64 : st;
  Column* col = Column::new_data_column(rst, 1);
  if (rst == SType::FLOAT32) {
    static_cast<FwColumn<float>*>(col)->set_elem(0, static_cast<float>(value));
  } else {
    static_cast<FwColumn<double>*>(col)->set_elem(0, value);
  }
  return colptr(col->repeat(nrows));
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
    colptr make_column(SType st, size_t nrows) const override;
};

const char* scalar_string_rn::value_type() const noexcept {
  return "string";
}

bool scalar_string_rn::valid_ltype(LType lt) const noexcept {
  return lt == LType::STRING;
}

colptr scalar_string_rn::make_column(SType st, size_t nrows) const {
  size_t len = value.size();
  SType rst = (st == SType::VOID)? SType::STR32 : st;
  size_t elemsize = (rst == SType::STR32)? 4 : 8;
  MemoryRange offbuf = MemoryRange::mem(2 * elemsize);
  if (elemsize == 4) {
    offbuf.set_element<uint32_t>(0, 0);
    offbuf.set_element<uint32_t>(1, static_cast<uint32_t>(len));
  } else {
    offbuf.set_element<uint64_t>(0, 0);
    offbuf.set_element<uint64_t>(1, len);
  }
  MemoryRange strbuf = MemoryRange::mem(len);
  std::memcpy(strbuf.xptr(), value.data(), len);
  Column* col = new_string_column(1, std::move(offbuf), std::move(strbuf));
  col->replace_rowindex(RowIndex(size_t(0), nrows, 0));
  return colptr(col);
}




//------------------------------------------------------------------------------
// collist_rn
//------------------------------------------------------------------------------

class collist_rn : public repl_node {
  intvec indices;

  public:
    explicit collist_rn(cols_intlist* cl) : indices(std::move(cl->indices)) {}
    void check_compatibility(size_t lrows, size_t lcols) const override;
    void replace_columns(workframe&, const intvec&) const override;
    void replace_values(workframe&, const intvec&) const override;
};


void collist_rn::check_compatibility(size_t, size_t lcols) const {
  size_t rcols = indices.size();
  if (rcols == 1 || rcols == lcols) return;
  throw ValueError() << "Cannot replace "
      << lcols << " column" << (lcols==1? "s" : "") << " with"
      << rcols << " column" << (rcols==1? "s" : "");
}


void collist_rn::replace_columns(workframe&, const intvec&) const {
  throw NotImplError() << "collist_rn::replace_columns()";
}
void collist_rn::replace_values(workframe&, const intvec&) const {
  throw NotImplError() << "collist_rn::replace_values()";
}




//------------------------------------------------------------------------------
// exprlist_rn
//------------------------------------------------------------------------------

class exprlist_rn : public repl_node {
  exprvec exprs;

  public:
    explicit exprlist_rn(cols_exprlist* cl) : exprs(std::move(cl->exprs)) {}
    void check_compatibility(size_t lrows, size_t lcols) const override;
    void replace_columns(workframe&, const intvec&) const override;
    void replace_values(workframe&, const intvec&) const override;
};


void exprlist_rn::check_compatibility(size_t, size_t lcols) const {
  size_t rcols = exprs.size();
  if (rcols == 1 || rcols == lcols) return;
  throw ValueError() << "Cannot replace "
      << lcols << " column" << (lcols==1? "s" : "") << " with"
      << rcols << " column" << (rcols==1? "s" : "");
}


void exprlist_rn::replace_columns(workframe& wf, const intvec& indices) const {
  DataTable* dt0 = wf.get_datatable(0);
  size_t lcols = indices.size();
  size_t rcols = exprs.size();
  xassert(lcols == rcols || rcols == 1);

  for (auto& expr : exprs) {
    expr->resolve(wf);
  }

  for (size_t i = 0; i < lcols; ++i) {
    size_t j = indices[i];
    Column* col = i < rcols? exprs[i]->evaluate_eager(wf).release()
                           : dt0->columns[indices[0]]->shallowcopy();
    xassert(col->nrows == dt0->nrows);
    delete dt0->columns[j];
    dt0->columns[j] = col;
  }
}


void exprlist_rn::replace_values(workframe&, const intvec&) const {
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


repl_node_ptr repl_node::make(workframe& wf, py::oobj src) {
  repl_node* res = nullptr;

  if (src.is_frame())       res = new frame_rn(src.to_datatable());
  else if (src.is_none())   res = new scalar_na_rn();
  else if (src.is_bool())   res = new scalar_int_rn(src.to_bool());
  else if (src.is_int())    res = new scalar_int_rn(src.to_int64());
  else if (src.is_float())  res = new scalar_float_rn(src.to_double());
  else if (src.is_string()) res = new scalar_string_rn(src.to_string());
  else if (is_PyBaseExpr(src) || src.is_list_or_tuple()) {
    auto cl = collist::make(wf, src, "replacement");
    auto intcl = dynamic_cast<cols_intlist*>(cl.get());
    auto expcl = dynamic_cast<cols_exprlist*>(cl.get());
    xassert(intcl || expcl);
    if (intcl) res = new collist_rn(std::move(intcl));
    if (expcl) res = new exprlist_rn(std::move(expcl));
  }
  else {
    throw TypeError()
      << "The replacement value of unknown type " << src.typeobj();
  }
  return repl_node_ptr(res);
}



}  // namespace dt
