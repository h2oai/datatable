//------------------------------------------------------------------------------
// Copyright 2019 H2O.ai
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
#include <string>
#include <vector>
#include "expr/declarations.h"
#include "column.h"
namespace dt {
namespace expr {


/**
  * `Workframe` is a "frame-in-progress": a collection of column
  * records that will at some point be converted into an actual
  * DataTable.
  *
  * Each column record contains the following information:
  *
  *   column    - the actual Column object
  *   name      - this column's name (or empty)
  *   frame_id  - if a column is added by reference, this will contain
  *               the index of the frame (within the evaluation
  *               context) where this column came from. If the column
  *               was NOT added by reference, this will contain -1.
  *   column_id - index of the column within the frame, if a column
  *               was added by reference.
  *
  * A column is considered to be "added by reference" if it's a copy
  * of one of the columns in one of the frames in the evaluation
  * context. For such columns we keep their original "address"
  * together with the column object. This allows us to refer back to
  * the original columns when performing certain operations such as
  * UPDATE or DELETE.
  *
  * A computed column is not added by reference, so for such column
  * the frame id is -1, and column id can be anything.
  *
  * Another possible column type is the "placeholder" column. These
  * columns have empty `column` object, and are used to denote new
  * or unresolved columns in a frame. For example, in the expression
  * `DT["A"] = 1` if there is no column "A" in the frame `DT`, the
  * expression will be resolved to a placeholder column named "A",
  * allowing us later to add such column in the UPDATE call.
  *
  */
class Workframe {
  private:
    struct Record {
      static constexpr uint32_t INVALID_FRAME = uint32_t(-1);
      Column      column;
      std::string name;
      uint32_t    frame_id;
      uint32_t    column_id;

      Record();
      Record(const Record&) = default;
      Record(Record&&) noexcept = default;
      Record& operator=(const Record&) = default;
      Record& operator=(Record&&) = default;
      Record(Column&&, std::string&&);
      Record(Column&&, const std::string&, size_t, size_t);
    };

    std::vector<Record> entries_;
    EvalContext& ctx_;
    Grouping grouping_mode_;

  public:
    Workframe(EvalContext&);
    Workframe(Workframe&&) = default;
    Workframe& operator=(Workframe&&) = default;
    Workframe(const Workframe&) = delete;
    Workframe& operator=(const Workframe&) = delete;

    void add_column(Column&& col, std::string&& name, Grouping grp);
    void add_ref_column(size_t iframe, size_t icol);
    void add_placeholder(const std::string& name, size_t iframe);
    void cbind(Workframe&&, bool at_end = true);
    void remove(const Workframe&);
    void rename(const std::string& name);

    size_t ncols() const noexcept;
    size_t nrows() const noexcept;
    EvalContext& get_context() const noexcept;

    bool is_computed_column(size_t i) const;
    bool is_reference_column(size_t i, size_t* iframe, size_t* icol) const;
    bool is_placeholder_column(size_t i) const;

    // For a single-column workframe: repeat the column `n` times
    void repeat_column(size_t n);

    // Reduce the number of columns down to `n`
    void truncate_columns(size_t n);

    void reshape_for_update(size_t target_nrows, size_t target_ncols);
    const Column& get_column(size_t i) const;
    std::string retrieve_name(size_t i);
    Column      retrieve_column(size_t i);
    void        replace_column(size_t i, Column&& col);

    std::unique_ptr<DataTable> convert_to_datatable() &&;

    // This method ensures that two `Workframe` objects have the
    // same grouping mode. It can either modify itself, or
    // `other` object.
    void sync_grouping_mode(Workframe& other);
    void sync_grouping_mode(Column& col, Grouping gmode);
    Grouping get_grouping_mode() const;
    void increase_grouping_mode(Grouping g);

  private:
    void column_increase_grouping_mode(Column&, Grouping from, Grouping to);

    friend class EvalContext;
};



}}  // namespace dt::expr
#endif
