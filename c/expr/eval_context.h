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
#ifndef dt_EXPR_EVAL_CONTEXT_h
#define dt_EXPR_EVAL_CONTEXT_h
#include <vector>            // std::vector
#include "expr/declarations.h"
#include "expr/expr.h"
#include "expr/py_by.h"      // py::oby
#include "expr/workframe.h"
#include "datatable.h"       // DataTable
#include "rowindex.h"        // RowIndex
namespace dt {
namespace expr {



/**
  * This is a main class used to evaluate expressions `DT[i, j, ...]`,
  * its purpose is to orchestrate the evaluation of all parts, and to
  * hold the information produced in the process.
  *
  * For inputs, this class holds `Expr` objects corresponding to each
  * part of the `DT[i,j]`: there is `iexpr_`, `jexpr_`, `byexpr_`,
  * `sortexpr_` and `rexpr_` (replacement). There are no join nodes
  * however: the join frames are stored into the `frames_` vector
  * directly. This may be expanded in the future when we allow joins
  * on arbitrary conditions.
  *
  * The `frames_` vector contains the list of frames that participate
  * in the evaluation. The first element of this vector is the root
  * frame (`DT`), and all subsequent elements are joined frames. Each
  * element in the `frames_` vector also contains a rowindex
  * describing which rows of that frame must be selected. The length
  * (nrows) of all these rowindices must be equal.
  *
  * The `groupby_` object specifies how the rows are split into
  * groups. This object may not be "empty", and as a fallback will
  * contain a single-group-all-rows Groupby.
  *
  * Additional elements:
  *
  * ungroup_rowindex_
  *   Can be used to take an existing grouped column (e.g. such as
  *   produced by a reducer), and expand it into a full-size column.
  *   This RowIndex is computed only on demand.
  *
  * group_rowindex_
  *   Can be used to take an existing full-size column and apply a
  *   a `first()` function to it, producing a "grouped" column.
  *
  * groupby_columns_
  *   Columns on which the frame was grouped. These columns may be
  *   either computed or "reference" columns. This Workframe is used
  *   for two purposes: (1) in order to detect whether a particular
  *   column is "group" column, and (2) in order to add the group
  *   columns at the beginning of the result frame.
  *
  * newnames_
  *   When a frame is updated, this vector will temporarily hold the
  *   names of the columns being created.
  *
  * eval_mode_
  *   Three conceptual operations are supported: SELECT, UPDATE and
  *   DELETE.
  *
  * add_groupby_columns_
  *   If this flag is false (it's true by default), then the groupby
  *   columns would not be added to the resulting frame.
  *
  */
class EvalContext
{
  struct subframe {
    DataTable* dt_;
    RowIndex   ri_;
    bool       natural_;  // was this frame joined naturally?
    size_t : 56;

    subframe(DataTable* dt, const RowIndex& ri, bool n)
      : dt_(dt), ri_(ri), natural_(n) {}
  };
  using frameVec = std::vector<subframe>;

  private:
    // Inputs
    Expr  iexpr_;
    Expr  jexpr_;
    Expr  byexpr_;
    Expr  sortexpr_;
    Expr  rexpr_;

    // Runtime
    frameVec   frames_;
    Groupby    groupby_;
    RowIndex   ungroup_rowindex_;
    RowIndex   group_rowindex_;
    Workframe  groupby_columns_;
    strvec     newnames_;
    EvalMode   eval_mode_;
    bool       add_groupby_columns_;
    size_t : 48;

  public:
    EvalContext(DataTable*, EvalMode = EvalMode::SELECT);
    EvalContext(const EvalContext&) = delete;
    EvalContext(EvalContext&&) = delete;

    EvalMode get_mode() const;

    void add_join(py::ojoin);
    void add_groupby(py::oby);
    void add_sortby(py::osort);
    void add_i(py::oobj);
    void add_j(py::oobj);
    void add_replace(py::oobj);

    py::oobj evaluate();

    DataTable* get_datatable(size_t i) const;
    const RowIndex& get_rowindex(size_t i) const;
    const Groupby& get_groupby();
    const RowIndex& get_ungroup_rowindex();
    const RowIndex& get_group_rowindex();
    bool is_naturally_joined(size_t i) const;
    bool has_groupby() const;
    bool has_group_column(size_t frame_index, size_t column_index) const;
    size_t nframes() const;
    size_t nrows() const;

    void apply_rowindex(const RowIndex& ri);
    void replace_groupby(Groupby&& gb_);
    void set_groupby_columns(Workframe&&);

  private:
    void compute_groupby_and_sort();

    py::oobj evaluate_delete();
    py::oobj evaluate_select();
    py::oobj evaluate_update();
    void evaluate_delete_columns();
    void evaluate_delete_rows();
    void evaluate_delete_subframe();
    intvec evaluate_j_as_column_index();
    void create_placeholder_columns();
    void typecheck_for_update(Workframe&, const intvec&);
    void update_groupby_columns(Grouping gmode);
};



}}  // namespace dt::expr
#endif
