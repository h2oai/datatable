//------------------------------------------------------------------------------
// Copyright 2020 H2O.ai
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
#include "column/ifelse.h"        // IfElse_ColumnImpl
#include "column/ifelsen.h"       // IfElseN_ColumnImpl
#include "expr/eval_context.h"    // EvalContext
#include "expr/fexpr_func.h"      // FExpr_Func
#include "expr/workframe.h"       // Workframe
#include "python/xargs.h"         // DECLARE_PYFN
#include "stype.h"                // SType
namespace dt {
namespace expr {



//------------------------------------------------------------------------------
// FExpr_IfElse
//------------------------------------------------------------------------------

class FExpr_IfElse : public FExpr_Func {
  private:
    // The number of conditions is 1 less than the number of values
    vecExpr conditions_;
    vecExpr values_;

  public:
    FExpr_IfElse(vecExpr&&, vecExpr&&);
    Workframe evaluate_n(EvalContext&) const override;
    std::string repr() const override;
};



FExpr_IfElse::FExpr_IfElse(vecExpr&& conditions, vecExpr&& values)
  : conditions_(std::move(conditions)),
    values_(std::move(values))
{
  xassert(conditions_.size() == values_.size() - 1);
}


Workframe FExpr_IfElse::evaluate_n(EvalContext& ctx) const {
  size_t n = conditions_.size();
  xassert(n >= 1);

  std::vector<Workframe> all_workframes;
  for (const auto& cond : conditions_) {
    all_workframes.push_back(cond->evaluate_n(ctx));
  }
  for (const auto& value : values_) {
    all_workframes.push_back(value->evaluate_n(ctx));
  }
  xassert(all_workframes.size() == 2*n + 1);
  for (size_t j = 0; j < all_workframes.size(); ++j) {
    if (all_workframes[j].ncols() != 1) {
      auto jj = (j < n)? j : j - n;
      throw TypeError() << "The `" << (j==jj? "condition" : "value")
          << (jj+1) << "` argument in ifelse() cannot be a multi-column "
                       "expression";
    }
  }
  auto gmode = Workframe::sync_grouping_mode(all_workframes);

  colvec condition_cols;
  for (size_t j = 0; j < n; ++j) {
    Column col = all_workframes[j].retrieve_column(0);
    if (col.stype() != SType::BOOL) {
      throw TypeError()
          << "The `condition" << (j+1)
          << "` argument in ifelse() must be a boolean column";
    }
    condition_cols.push_back(std::move(col));
  }

  colvec value_cols;
  SType out_stype = SType::VOID;
  for (size_t j = n; j < all_workframes.size(); ++j) {
    Column col = all_workframes[j].retrieve_column(0);
    out_stype = common_stype(out_stype, col.stype());
    value_cols.push_back(std::move(col));
  }
  for (Column& col : value_cols) {
    col.cast_inplace(out_stype);
  }

  Workframe wf_out(ctx);
  if (n == 1) {
    wf_out.add_column(Column{
      new IfElse_ColumnImpl(
          std::move(condition_cols[0]),
          std::move(value_cols[0]),
          std::move(value_cols[1]))
      }, std::string(), gmode
    );
  } else {
    wf_out.add_column(
      Column{
        new IfElseN_ColumnImpl(std::move(condition_cols), std::move(value_cols))
      }, std::string(), gmode
    );
  }
  return wf_out;
}


std::string FExpr_IfElse::repr() const {
  std::string out = "ifelse(";
  for (size_t i = 0; i < conditions_.size(); ++i) {
    out += conditions_[i]->repr();
    out += ", ";
    out += values_[i]->repr();
    out += ", ";
  }
  out += values_.back()->repr();
  out += ')';
  return out;
}




//------------------------------------------------------------------------------
// Python interface
//------------------------------------------------------------------------------

static const char* doc_ifelse =
R"(ifelse(condition1, value1, condition2, value2, ..., default)
--
.. x-version-added:: 0.11.0

An expression that chooses its value based on one or more
conditions.

This is roughly equivalent to the following Python code::

    >>> result = value1 if condition1 else \
    ...          value2 if condition2 else \
    ...          ...                  else \
    ...          default

For every row this function evaluates the smallest number of expressions
necessary to get the result. Thus, it evaluates `condition1`, `condition2`,
and so on until it finds the first condition that evaluates to `True`.
It then computes and returns the corresponding `value`. If all conditions
evaluate to `False`, then the `default` value is computed and returned.

Also, if any of the conditions produces NA then the result of the expression
also becomes NA without evaluating any further conditions or values.


Parameters
----------
condition1, condition2, ...: FExpr[bool]
    Expressions each producing a single boolean column. These conditions
    will be evaluated in order until we find the one equal to `True`.

value1, value2, ...: FExpr
    Values that will be used when the corresponding condition evaluates
    to `True`. These must be single columns.

default: FExpr
    Value that will be used when all conditions evaluate to `False`.
    This must be a single column.

return: FExpr
    The resulting expression is a single column whose stype is the
    common stype for all `value1`, ..., `default` columns.


Notes
-----
.. x-version-changed:: 1.0.0

    Earlier this function accepted a single condition only.

Examples
--------

Single condition
~~~~~~~~~~~~~~~~
Task: Create a new column `Colour`, where if `Set` is `'Z'` then the
value should be `'Green'`, else `'Red'`::

    >>> from datatable import dt, f, by, ifelse, update
    >>>
    >>> df = dt.Frame("""Type       Set
    ...                   A          Z
    ...                   B          Z
    ...                   B          X
    ...                   C          Y""")
    >>> df[:, update(Colour = ifelse(f.Set == "Z",  # condition
    ...                              "Green",       # if condition is True
    ...                              "Red"))        # if condition is False
    ... ]
    >>> df
       | Type   Set    Colour
       | str32  str32  str32
    -- + -----  -----  ------
     0 | A      Z      Green
     1 | B      Z      Green
     2 | B      X      Red
     3 | C      Y      Red
    [4 rows x 3 columns]


Multiple conditions
~~~~~~~~~~~~~~~~~~~
Task: Create new column ``value`` whose value is taken from columns ``a``,
``b``, or ``c`` -- whichever is nonzero first::

    >>> df = dt.Frame({"a": [0,0,1,2],
    ...                "b": [0,3,4,5],
    ...                "c": [6,7,8,9]})
    >>> df
       |     a      b      c
       | int32  int32  int32
    -- + -----  -----  -----
     0 |     0      0      6
     1 |     0      3      7
     2 |     1      4      8
     3 |     2      5      9
    [4 rows x 3 columns]

    >>> df['value'] = ifelse(f.a > 0, f.a,  # first condition and result
    ...                      f.b > 0, f.b,  # second condition and result
    ...                      f.c)           # default if no condition is True
    >>> df
       |     a      b      c  value
       | int32  int32  int32  int32
    -- + -----  -----  -----  -----
     0 |     0      0      6      6
     1 |     0      3      7      3
     2 |     1      4      8      1
     3 |     2      5      9      2
    [4 rows x 4 columns]
)";

static py::oobj ifelse(const py::XArgs& args) {
  auto n = args.num_varargs();
  if (n < 3) {
    throw TypeError()
      << "Function `datatable.ifelse()` requires at least 3 arguments";
  }
  if (n % 2 != 1) {
    throw TypeError()
      << "Missing the required `default` argument in function "
         "`datatable.ifelse()`";
  }
  vecExpr conditions;
  vecExpr values;
  for (size_t i = 0; i < n/2; i++) {
    conditions.push_back(as_fexpr(args.vararg(i*2)));
    values.push_back(as_fexpr(args.vararg(i*2 + 1)));
  }
  values.push_back(as_fexpr(args.vararg(n - 1)));
  return dt::expr::PyFExpr::make(
              new dt::expr::FExpr_IfElse(std::move(conditions),
                                         std::move(values))
         );
}

DECLARE_PYFN(&ifelse)
    ->name("ifelse")
    ->docs(doc_ifelse)
    ->allow_varargs();



}}  // dt::expr
