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
#include "expr/expr.h"
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

  OColumn col0;
  if (rcols == 1) {
    col0 = dtr->get_ocolumn(0);  // copy
    // Avoid resizing `col0` multiple times in the loop below
    if (rrows == 1) {
      col0->resize_and_fill(lrows);  // TODO: use function from repeat.cc
    }
  }
  for (size_t i = 0; i < lcols; ++i) {
    size_t j = indices[i];
    OColumn coli = (rcols == 1)? col0 : dtr->get_ocolumn(i);  // copy
    if (coli.nrows() == 1) {
      coli->resize_and_fill(lrows);  // TODO: use function from repeat.cc
    }
    dt0->set_ocolumn(j, std::move(coli));
  }
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
    const OColumn& coli = dtr->get_ocolumn(rcols == 1? 0 : i);
    if (!dt0->get_ocolumn(j)) {
      dt0->set_ocolumn(j,
          OColumn::new_na_column(coli.stype(), dt0->nrows));
    }
    OColumn& colj = dt0->get_ocolumn(j);
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
    void replace_columns(workframe&, const intvec&) const override;
    void replace_values(workframe&, const intvec&) const override;

  protected:
    void check_column_types(const DataTable*, const intvec&) const;
    virtual const char* value_type() const noexcept = 0;
    virtual bool valid_ltype(LType lt) const noexcept = 0;
    virtual OColumn make_column(SType st, size_t nrows) const = 0;
};


void scalar_rn::check_compatibility(size_t, size_t) const {}


void scalar_rn::check_column_types(
  const DataTable* dt0, const intvec& indices) const
{
  for (size_t j : indices) {
    const OColumn& col = dt0->get_ocolumn(j);
    if (col && !valid_ltype(col.ltype())) {
      throw TypeError() << "Cannot assign " << value_type()
        << " value to column `" << dt0->get_names()[j]
        << "` of type " << col.stype();
    }
  }
}


void scalar_rn::replace_columns(workframe& wf, const intvec& indices) const {
  DataTable* dt0 = wf.get_datatable(0);
  check_column_types(dt0, indices);

  std::unordered_map<SType, OColumn, EnumClassHash> new_columns;
  for (size_t j : indices) {
    const OColumn& col = dt0->get_ocolumn(j);
    SType stype = col? col.stype() : SType::VOID;
    if (new_columns.count(stype) == 0) {
      new_columns[stype] = make_column(stype, dt0->nrows);
    }
    OColumn newcol = new_columns[stype];  // copy
    dt0->set_ocolumn(j, std::move(newcol));
  }
}


void scalar_rn::replace_values(workframe& wf, const intvec& indices) const {
  DataTable* dt0 = wf.get_datatable(0);
  const RowIndex& ri0 = wf.get_rowindex(0);
  check_column_types(dt0, indices);

  for (size_t j : indices) {
    const OColumn& colj = dt0->get_ocolumn(j);
    SType stype = colj? colj.stype() : SType::VOID;
    OColumn replcol = make_column(stype, 1);
    stype = replcol.stype();  // may change from VOID to BOOL, FIXME!
    if (!colj) {
      dt0->set_ocolumn(j, OColumn::new_na_column(stype, dt0->nrows));
    }
    else if (colj.stype() != stype) {
      dt0->set_ocolumn(j, colj.cast(stype));
    }
    OColumn& ocol = dt0->get_ocolumn(j);
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
    OColumn make_column(SType st, size_t nrows) const override;
};

const char* scalar_na_rn::value_type() const noexcept {
  return "None";  // LCOV_EXCL_LINE
}


bool scalar_na_rn::valid_ltype(LType) const noexcept {
  return true;
}


OColumn scalar_na_rn::make_column(SType st, size_t nrows) const {
  if (st == SType::VOID) st = SType::BOOL;
  return OColumn::new_na_column(st, nrows);
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
    OColumn make_column(SType st, size_t nrows) const override;
    template <typename T> OColumn _make1(SType st) const;
};


const char* scalar_int_rn::value_type() const noexcept {
  return "integer";
}


bool scalar_int_rn::valid_ltype(LType lt) const noexcept {
  return lt == LType::INT || lt == LType::REAL ||
         (lt == LType::BOOL && (value == 0 || value == 1));
}


OColumn scalar_int_rn::make_column(SType st, size_t nrows) const {
  int64_t av = std::abs(value);
  SType rst = value == 0 || value == 1? SType::BOOL :
              av <= 127? SType::INT8 :
              av <= 32767? SType::INT16 :
              av <= 2147483647? SType::INT32 : SType::INT64;
  if (static_cast<size_t>(st) > static_cast<size_t>(rst)) {
    rst = st;
  }
  OColumn col1 = rst == SType::BOOL? _make1<int8_t>(rst) :
                 rst == SType::INT8? _make1<int8_t>(rst) :
                 rst == SType::INT16? _make1<int16_t>(rst) :
                 rst == SType::INT32? _make1<int32_t>(rst) :
                 rst == SType::INT64? _make1<int64_t>(rst) :
                 rst == SType::FLOAT32? _make1<float>(rst) :
                 rst == SType::FLOAT64? _make1<double>(rst) : OColumn();
  xassert(col1);
  return col1->repeat(nrows);
}


template <typename T>
OColumn scalar_int_rn::_make1(SType stype) const {
  MemoryRange mbuf = MemoryRange::mem(sizeof(T));
  mbuf.set_element<T>(0, static_cast<T>(value));
  return OColumn::new_mbuf_column(stype, std::move(mbuf));
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
    OColumn make_column(SType st, size_t nrows) const override;
};


const char* scalar_float_rn::value_type() const noexcept {
  return "float";
}


bool scalar_float_rn::valid_ltype(LType lt) const noexcept {
  return lt == LType::REAL;
}


OColumn scalar_float_rn::make_column(SType st, size_t nrows) const {
  constexpr double MAX = double(std::numeric_limits<float>::max());
  // st can be either VOID or FLOAT32 or FLOAT64
  // VOID we always convert into FLOAT64 so as to avoid loss of precision;
  // otherwise we attempt to keep the old type `st`, unless doing so will lead
  // to value truncation (value does not fit into float32).
  bool res64 = (st == SType::FLOAT64 || st == SType::VOID ||
                std::abs(value) > MAX);
  SType result_stype = res64? SType::FLOAT64 : SType::FLOAT32;

  MemoryRange mbuf = MemoryRange::mem(res64? sizeof(double) : sizeof(float));
  if (res64) {
    mbuf.set_element<double>(0, value);
  } else {
    mbuf.set_element<float>(0, static_cast<float>(value));
  }
  return OColumn::new_mbuf_column(result_stype, std::move(mbuf))->repeat(nrows);
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
    OColumn make_column(SType st, size_t nrows) const override;
};

const char* scalar_string_rn::value_type() const noexcept {
  return "string";
}

bool scalar_string_rn::valid_ltype(LType lt) const noexcept {
  return lt == LType::STRING;
}

OColumn scalar_string_rn::make_column(SType st, size_t nrows) const {
  if (nrows == 0) {
    return OColumn::new_data_column(SType::STR32, 0);
  }
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
  OColumn col = new_string_column(1, std::move(offbuf), std::move(strbuf));
  if (nrows > 1) {
    col->replace_rowindex(RowIndex(size_t(0), nrows, 0));
  }
  return col;
}




//------------------------------------------------------------------------------
// collist_rn
//------------------------------------------------------------------------------

class collist_rn : public repl_node {
  intvec indices;

  public:
    explicit collist_rn(collist* cl) : indices(cl->release_indices()) {}
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
    explicit exprlist_rn(collist* cl) : exprs(cl->release_exprs()) {}
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
    OColumn col = (i < rcols)? exprs[i]->evaluate_eager(wf)
                             : dt0->get_ocolumn(indices[0]);
    xassert(col.nrows() == dt0->nrows);
    dt0->set_ocolumn(j, std::move(col));
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
  else if (src.is_dtexpr() || src.is_list_or_tuple()) {
    collist cl(wf, src, collist::REPL_NODE);
    if (cl.is_simple_list()) res = new collist_rn(&cl);
    else                     res = new exprlist_rn(&cl);
  }
  else {
    throw TypeError()
      << "The replacement value of unknown type " << src.typeobj();
  }
  return repl_node_ptr(res);
}



}  // namespace dt
