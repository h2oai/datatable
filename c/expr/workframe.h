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
#include "expr/eval_context.h"
namespace dt {
namespace expr {


/**
  *
  */
class Workframe {
  private:
    struct Info {
      static constexpr uint32_t INVALID_FRAME = uint32_t(-1);
      std::string name;
      uint32_t    frame_id;
      uint32_t    column_id;

      Info();
      Info(Info&&) noexcept = default;
      Info(std::string&&);
      Info(std::string&&, size_t, size_t);
      Info(const std::string&, size_t, size_t);
    };

    EvalContext& ctx;
    std::vector<Column> columns;
    std::vector<Info>   infos;
    Grouping grouping_mode;

  public:
    Workframe(EvalContext&);
    Workframe(const Workframe&) = delete;
    Workframe(Workframe&&) = default;

    void add(Column&& col, std::string&& name, Grouping grp);
    void add(Column&& col, Grouping grp = Grouping::GtoALL);
    void add_ref_column(size_t iframe, size_t icol);
    void append(Workframe&&);
    void replace_name(const std::string& name);

    size_t size() const noexcept;
    EvalContext& get_workframe() const noexcept;
    Column& get_column(size_t i);
    std::string& get_name(size_t i);
    colvec& get_columns();

    std::unique_ptr<DataTable> convert_to_datatable();

    // This method ensures that two `Workframe` objects have the
    // same grouping mode. It can either modify itself, or
    // `other` object.
    void sync_grouping_mode(Workframe& other);
    void sync_grouping_mode(Column& col, Grouping gmode);
    Grouping get_grouping_mode() const;

  private:
    void increase_grouping_mode(Grouping g);
    void column_increase_grouping_mode(Column&, Grouping from, Grouping to);
};



}}  // namespace dt::expr
#endif
