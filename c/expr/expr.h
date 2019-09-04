//------------------------------------------------------------------------------
// Copyright 2018-2019 H2O.ai
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
#ifndef dt_EXPR_EXPR_h
#define dt_EXPR_EXPR_h
#include <memory>
#include <string>
#include <vector>
#include "expr/head.h"
#include "expr/op.h"
#include "expr/workframe.h"
#include "python/obj.h"
#include "column.h"
namespace dt {
namespace expr {


class Expr;
class Outputs;
using vecExpr = std::vector<Expr>;



//------------------------------------------------------------------------------
// Expr
//------------------------------------------------------------------------------

class Expr {
  private:
    ptrHead  head;
    vecExpr  inputs;

  public:
    explicit Expr(py::robj src);
    Expr() = default;
    Expr(Expr&&) = default;
    Expr(const Expr&) = delete;

    Head::Kind get_expr_kind() const;
    bool evaluate_as_bool() const;

    Outputs evaluate(workframe& wf) const;
    Outputs evaluate_f(workframe& wf, size_t frame_id) const;

  private:
    // Construction helpers
    void _init_from_bool(py::robj);
    void _init_from_dictionary(py::robj);
    void _init_from_dtexpr(py::robj);
    void _init_from_float(py::robj);
    void _init_from_frame(py::robj);
    void _init_from_int(py::robj);
    void _init_from_iterable(py::robj);
    void _init_from_list(py::robj);
    void _init_from_none();
    void _init_from_numpy(py::robj);
    void _init_from_pandas(py::robj);
    void _init_from_slice(py::robj);
    void _init_from_string(py::robj);
    void _init_from_type(py::robj);
};





//------------------------------------------------------------------------------
//------------------------------------------------------------------------------


class base_expr;
using pexpr = std::unique_ptr<base_expr>;




//------------------------------------------------------------------------------
// dt::expr::base_expr
//------------------------------------------------------------------------------

class base_expr {
  public:
    base_expr();
    virtual ~base_expr();
    virtual SType resolve(const workframe&) = 0;
    virtual GroupbyMode get_groupby_mode(const workframe&) const = 0;
    virtual Column evaluate(workframe&) = 0;

    virtual bool is_columnset_expr() const;
    virtual bool is_literal_expr() const;
    virtual bool is_negated_expr() const;
    virtual pexpr get_negated_expr();
    virtual py::oobj get_literal_arg();
};


// Initialize once
void init_expr();


py::oobj make_pyexpr(Op opcode, py::oobj arg);
py::oobj make_pyexpr(Op opcode, py::otuple args, py::otuple params);



}}  // namespace dt::expr


// hashing support
namespace std {
  template <> struct hash<dt::expr::Op> {
    std::size_t operator()(const dt::expr::Op& op) const {
      return static_cast<size_t>(op);
    }
  };
}

#endif
