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
#ifndef dt_EXPR_WORKFRAME_h
#define dt_EXPR_WORKFRAME_h
#include <vector>        // std::vector
#include "datatable.h"   // DataTable
#include "groupby.h"     // Groupby
#include "rowindex.h"    // RowIndex
namespace dt {



/**
 * This is a main class used to evaluate the expression `DT[i, j, ...]`. This
 * class is essentially a "work-in-progress Frame", hence the name. It is not
 * a real frame, however, in the sense that it may be misshapen, or violate
 * other Frame constraints at some point in its lifetime.
 *
 * Initially, a workframe is created using the DataTable* object `DT` which is
 * at the root of the expression `DT[i, j, ...]`.
 *
 * Later, `join_node`(s) may append additional DataTable* objects, with all
 * `RowIndex`es specifying how they merge together.
 *
 * The `by_node` will attach a `Groupby` object. The Groupby will specify how
 * the rows are split into groups, and it applies to all `DataTable*`s in the
 * workframe. The shape of the Groupby must be compatible with all current
 * `RowIndex`es.
 *
 * An `i_node` applies a new row filter to all Frames in the workframe. If
 * there is a non-empty Groupby, the `i_node` must apply within each group, and
 * then update the Groupby to account for the new configuration of the rows.
 */
class workframe {
  private:
    std::vector<DataTable*> dts;
    std::vector<RowIndex> ris;
    Groupby gb;

  public:
    workframe() = delete;
    workframe(const workframe&) = delete;
    workframe(workframe&&) = delete;

    explicit workframe(DataTable*);

    DataTable* get_datatable(size_t id) const;
    const RowIndex& get_rowindex(size_t id) const;
    size_t nframes() const;
    size_t nrows() const;

    void apply_rowindex(const RowIndex& ri);
};


}
#endif
