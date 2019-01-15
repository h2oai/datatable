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
#include "expr/collist.h"
#include "expr/repl_node.h"
#include "utils/exceptions.h"

namespace dt {


//------------------------------------------------------------------------------
// frame_rn
//------------------------------------------------------------------------------

class frame_rn : public repl_node {
  DataTable* dt;

  public:
    frame_rn(DataTable* dt_) : dt(dt_) {}
    void check_compatibility(size_t lrows, size_t lcols) const;
};


void frame_rn::check_compatibility(size_t lrows, size_t lcols) const {
  size_t rrows = dt->nrows;
  size_t rcols = dt->ncols;
  if ((rrows == lrows || rrows == 1) && (rcols == lcols || rcols == 1)) return;
  if (rcols == 0 && lcols == 0 && rrows == 0) return;
  throw ValueError() << "Invalid replacement Frame: expected [" <<
      lrows << " x " << lcols << "], but received [" << rrows <<
      " x " << rcols << "]";

}



//------------------------------------------------------------------------------
// scalar_rn
//------------------------------------------------------------------------------

class scalar_na_rn : public repl_node {
};


class scalar_int_rn : public repl_node {
  int64_t value;

  public:
    scalar_int_rn(int64_t x) : value(x) {}
};


class scalar_float_rn : public repl_node {
  double value;

  public:
    scalar_float_rn(double x) : value(x) {}
};


class scalar_string_rn : public repl_node {
  std::string value;

  public:
    scalar_string_rn(std::string&& x) : value(std::move(x)) {}
};




//------------------------------------------------------------------------------
// collist_rn
//------------------------------------------------------------------------------

class collist_rn : public repl_node {
  intvec indices;

  public:
    collist_rn(cols_intlist* cl) : indices(std::move(cl->indices)) {}
};




//------------------------------------------------------------------------------
// exprlist_rn
//------------------------------------------------------------------------------

class exprlist_rn : public repl_node {
  exprvec exprs;

  public:
    exprlist_rn(cols_exprlist* cl) : exprs(std::move(cl->exprs)) {}
};




//------------------------------------------------------------------------------
// dt::repl_node
//------------------------------------------------------------------------------

repl_node::~repl_node() {}


repl_node_ptr repl_node::make(py::oobj src, workframe& wf) {
  repl_node* res = nullptr;

  if (src.is_frame())       res = new frame_rn(src.to_frame());
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
