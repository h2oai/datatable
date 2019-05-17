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
#include "utils/function.h"
namespace dt {
namespace expr {


/**
 * `base_expr` class that implements computation of unary operators
 * and single-argument functions.
 */
class expr_unaryop : public base_expr {
  private:
    pexpr arg;
    Op opcode;

  public:
    expr_unaryop(pexpr&& a, Op op);
    SType resolve(const workframe& wf) override;
    GroupbyMode get_groupby_mode(const workframe&) const override;
    colptr evaluate_eager(workframe& wf) override;

    bool is_negated_expr() const override;
    pexpr get_negated_expr() override;
};



/**
 * Python-facing function that implements an unary operator / single-
 * argument function. This function can take as an argument either a
 * python scalar, or an f-expression, or a Frame (in which case the
 * function is applied to all elements of the frame).
 */
py::oobj unary_pyfn(const py::PKArgs&);




//------------------------------------------------------------------------------
// unary_infos
//------------------------------------------------------------------------------

class unary_infos {
  using unary_func_t = void(*)(size_t nrows, const void* inp, void* out);

  public:
    struct uinfo {
      unary_func_t fn;
      union {
        double(*d_d)(double);
        bool(*d_b)(double);
        int64_t(*l_l)(int64_t);
        bool(*l_b)(int64_t);
      } scalarfn;
      SType output_stype;
      SType cast_stype;
      size_t : 48;
    };

    unary_infos();
    const uinfo* get_infon(Op opcode, SType input_stype) const;
    const uinfo& get_infox(Op opcode, SType input_stype) const;
    Op resolve_opcode(const py::PKArgs&) const;

    template <float(*F32)(float), double(*F64)(double)>
    inline void add_math(Op opcode, const std::string& name);

  private:
    std::unordered_map<size_t /* Op, SType */, uinfo> info;
    std::unordered_map<size_t /* Op */, std::string> names;
    std::unordered_map<const py::PKArgs*, Op> opcodes;
    Op current_opcode;  // used only when registering opcodes

    static constexpr size_t id(Op) noexcept;
    static constexpr size_t id(Op, SType) noexcept;
    void set_name(Op, const std::string&);
    void add(Op, SType input_stype, SType output_stype, unary_func_t fn,
             SType cast=SType::VOID);

    void register_op(Op opcode, const std::string& name, const py::PKArgs&,
                     dt::function<void()>);
    void add_copy_converter(SType in, SType out = SType::VOID);
    void add_converter(SType in, SType out, unary_func_t);
    void add_converter(void(*)(size_t, const int8_t*, int8_t*));
    void add_converter(void(*)(size_t, const int16_t*, int16_t*));
    void add_converter(void(*)(size_t, const int32_t*, int32_t*));
    void add_converter(void(*)(size_t, const int64_t*, int64_t*));
    void add_converter(void(*)(size_t, const float*, float*));
    void add_converter(void(*)(size_t, const double*, double*));
    void add_scalarfn_l(int64_t(*)(int64_t));
    void add_scalarfn_l(bool(*)(int64_t));
    void add_scalarfn_d(double(*)(double));
    void add_scalarfn_d(bool(*)(double));
};


extern unary_infos unary_library;



}}
#endif
