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
#ifndef dt_EXPR_EVAL_CONTEXT_h
#define dt_EXPR_EVAL_CONTEXT_h
#include <vector>            // std::vector
#include "expr/by_node.h"    // py::oby, by_node_ptr
#include "expr/expr.h"
#include "expr/j_node.h"     // j_node_ptr
#include "expr/join_node.h"  // py::ojoin
#include "expr/sort_node.h"  // py::osort
#include "datatable.h"       // DataTable
#include "rowindex.h"        // RowIndex
namespace dt {

namespace expr { class expr_column; }


struct subframe {
  DataTable* dt;
  RowIndex ri;
  bool natural;  // was this frame joined naturally?
  size_t : 56;
};
using frvec = std::vector<subframe>;

enum class EvalMode : uint8_t {
  SELECT,
  UPDATE,
  DELETE
};



/**
 * This is a main class used to evaluate the expression `DT[i, j, ...]`. This
 * class is essentially a "work-in-progress Frame", hence the name. It is not
 * a real frame, however, in the sense that it may be misshapen, or violate
 * other Frame constraints at some point in its lifetime.
 *
 * Initially, a EvalContext is created using the DataTable* object `DT` which is
 * at the root of the expression `DT[i, j, ...]`.
 *
 * Later, `join_node`(s) may append additional DataTable* objects, with all
 * `RowIndex`es specifying how they merge together.
 *
 * The `by_node` will attach a `Groupby` object. The Groupby will specify how
 * the rows are split into groups, and it applies to all `DataTable*`s in the
 * EvalContext. The shape of the Groupby must be compatible with all current
 * `RowIndex`es.
 *
 * An `i_node` applies a new row filter to all Frames in the EvalContext. If
 * there is a non-empty Groupby, the `i_node` must apply within each group, and
 * then update the Groupby to account for the new configuration of the rows.
 */
class EvalContext {
  private:
    // Inputs
    by_node       byexpr;  // old
    j_node_ptr    jexpr;   // old
    expr::Expr    iexpr_;
    expr::Expr    jexpr_;
    expr::Expr    repl_;

    // Runtime
    frvec         frames;
    Groupby       gb;
    EvalMode      mode;
    GroupbyMode   groupby_mode;
    size_t : 48;

    // Result
    colvec columns;
    strvec newnames;
    strvec colnames;
    struct ritriple { RowIndex ab, bc, ac; };
    std::vector<ritriple> all_ri;
    std::unique_ptr<DataTable> out_datatable;

  public:
    EvalContext(DataTable*, EvalMode);
    EvalContext(const EvalContext&) = delete;
    EvalContext(EvalContext&&) = delete;

    EvalMode get_mode() const;
    GroupbyMode get_groupby_mode() const;

    void add_join(py::ojoin);
    void add_groupby(py::oby);
    void add_sortby(py::osort);
    void add_i(py::oobj);
    void add_j(py::oobj);
    void add_replace(py::oobj);

    void evaluate();
    py::oobj get_result();

    DataTable* get_datatable(size_t i) const;
    const RowIndex& get_rowindex(size_t i) const;
    const Groupby& get_groupby();
    const by_node& get_by_node() const;
    bool is_naturally_joined(size_t i) const;
    bool has_groupby() const;
    size_t nframes() const;
    size_t nrows() const;

    void apply_rowindex(const RowIndex& ri);
    void apply_groupby(const Groupby& gb_);

    size_t size() const noexcept;
    void reserve(size_t n);
    void add_column(Column&&, const RowIndex&, std::string&&);

    void fix_columns();

  private:
    void evaluate_delete();
    void evaluate_delete_columns();
    void evaluate_delete_rows();
    void evaluate_delete_subframe();
    void evaluate_update();
    intvec evaluate_j_as_column_index();
    void create_placeholder_columns();
    void typecheck_for_update(expr::Workframe&, const intvec&);

    friend class by_node;  // Allow access to `gb`
};



}  // namespace dt
#endif
