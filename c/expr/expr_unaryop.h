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
#ifndef dt_EXPR_EXPR_UNARYOP_h
#define dt_EXPR_EXPR_UNARYOP_h
#include "expr/expr.h"
#include "python/_all.h"
namespace dt {
namespace expr {


class expr_unaryop : public base_expr {
  private:
    pexpr arg;
    Op opcode;

  public:
    expr_unaryop(pexpr&& a, Op op);
    bool is_negated_expr() const override;
    pexpr get_negated_expr() override;
    SType resolve(const workframe& wf) override;
    GroupbyMode get_groupby_mode(const workframe&) const override;
    colptr evaluate_eager(workframe& wf) override;
};



//------------------------------------------------------------------------------
// unary_infos
//------------------------------------------------------------------------------



class unary_infos {
  using unary_func_t = void(*)(Op op, size_t nrows, const void* inp, void* out);
  public:
    struct uinfo {
      unary_func_t fn;
      SType output_stype;
      size_t : 56;
    };

    unary_infos();
    const uinfo& xget(Op opcode, SType input_stype) const;

  private:
    std::unordered_map<size_t, uinfo> info;
    std::unordered_map<size_t, std::string> names;

    static constexpr size_t id(Op) noexcept;
    static constexpr size_t id(Op, SType) noexcept;
    void set_name(Op, const std::string&);
    void add(Op, SType, SType, unary_func_t);
};


extern unary_infos unary_library;



}}
#endif
