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
    vcolptr evaluate_lazy(workframe& wf) override;

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

/**
 * Singleton class (the instance is `unary_library`) containing the
 * information about all unary functions, and how they should be
 * applied.
 *
 * Specifically, a unary function is selected via an opcode + the
 * argument's stype. For example, `abs(x)` is a unary function with
 * opcode `Op::ABS` and several overloads for inputs of types INT8,
 * INT16, INT32, INT64, FLOAT32, FLOAT64, etc.
 *
 * Public API
 * ----------
 *
 * - get_infon(opcode, input_stype) -> const uinfo*
 * - get_infox(opcode, input_stype) -> const uinfo&
 *
 *     For a given opcode+stype return the record describing this
 *     unary function. The two methods only differ in their behavior
 *     when such record is not found: `get_infon` returns a nullptr
 *     in this case, whereas `get_infox` throws an exception.
 *
 *     See below for the description of `uinfo` object returned from
 *     this function.
 *
 * - get_opcode_from_args(args) -> Op
 *
 *     If a function for a certain opcode was exported into python,
 *     then such function will have an associated `py::PKArgs`
 *     object. The method `get_opcode_from_args` retrieves the
 *     opcode corresponding to this PKArgs object.
 *
 *     This method is only used inside `unary_pyfn()`, allowing us
 *     to reuse the same function (albeit with different PKArgs)
 *     for all unary funcs exposed into python.
 *
 * uinfo
 * -----
 * Information about each valid unary function is stored in the
 * `uinfo` struct, which has the following fields:
 *
 * - vectorfn (unary_func_t)
 *
 *     Function that can be applied to a vector of inputs in order
 *     to produce the outputs. The signature of this function is
 *     `void(*)(size_t nrows, const void* input, void* output)`.
 *
 *     The `input` and `output` arrays are type-erased in the
 *     function's signature, but internally it is expected to know
 *     what the actual types are. Often, `vectorfn` will be declared
 *     as a function with concrete types, and then reinterpret-casted
 *     into `unary_func_t` in order to store in the `uinfo` struct.
 *
 *     This function will be called with `input` pointing to an
 *     existing contiguous input data array, and likewise `output`
 *     will be pre-allocated for `nrows` elements too, according to
 *     the `output_stype` field.
 *
 *     This field could be `nullptr`, indicating that no transform
 *     is necessary: the output can be simply copied from the input.
 *
 * - scalarfn (union)
 *
 *     Function that can be applied to a single (scalar) input. In
 *     order to accommodate multiple possible signatures within the
 *     same `uinfo` struct, we use a union here. A particular member
 *     of the union will be selected according to the type of the
 *     input and the `output_stype` field.
 *
 *     Specifically, the following signatures are recognized:
 *
 *         (double | int64_t) -> (double | bool)
 *
 *     The input type corresponds to python `float` and `int` types,
 *     the output is wrapped as python `float` or `bool`. In the
 *     future we may add support for more signatures, as the need
 *     arises.
 *
 *     This field can be `nullptr` indicating that the ufunc cannot
 *     be applied to this input type. This field will be `nullptr`
 *     for `uinfo`s corresponding to all input stypes other than
 *     INT64 or FLOAT64. If this field is non-null, then output_stype
 *     must be either FLOAT64 or BOOL.
 *
 * - output_stype (SType)
 *
 *     The SType of the output column produced from this ufunc.
 *
 * - cast_stype (SType)
 *
 *     If this field is non-empty (empty being SType::VOID), then the
 *     input column must be cast into this stype before being passed
 *     to `vectorfn` (or before being returned, if `vectorfn` is
 *     nullptr).
 *
 */
class unary_infos {
  using unary_func_t = void(*)(size_t nrows, const void* inp, void* out);

  public:
    struct uinfo {
      unary_func_t vectorfn;
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
    Op get_opcode_from_args(const py::PKArgs&) const;

  private:
    std::unordered_map<size_t /* Op, SType */, uinfo> info;
    std::unordered_map<size_t /* Op */, std::string> names;
    std::unordered_map<const py::PKArgs*, Op> opcodes;
    Op current_opcode;  // used only when registering opcodes

    static constexpr size_t id(Op) noexcept;
    static constexpr size_t id(Op, SType) noexcept;
    void add(Op, SType input_stype, SType output_stype, unary_func_t fn,
             SType cast=SType::VOID);

    void register_op(Op opcode, const std::string& name, const py::PKArgs*,
                     dt::function<void()>);
    void add_vectorfn(SType in, SType out, unary_func_t);
    void add_scalarfn_l(int64_t(*)(int64_t));
    void add_scalarfn_l(bool(*)(int64_t));
    void add_scalarfn_d(double(*)(double));
    void add_scalarfn_d(bool(*)(double));

    template <float(*F32)(float), double(*F64)(double)>
    void register_math_op(Op, const std::string&, const py::PKArgs&);
};


extern unary_infos unary_library;



}}
#endif
