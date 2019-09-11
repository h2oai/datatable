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
#include "expr/outputs.h"
#include "column_impl.h"
#include "datatable.h"
namespace dt {
namespace expr {


//------------------------------------------------------------------------------
// Outputs
//------------------------------------------------------------------------------

Outputs::Outputs(EvalContext& ctx_)
  : ctx(ctx_),
    grouping_mode(Grouping::SCALAR) {}



void Outputs::add(Column&& col, std::string&& name, Grouping gmode) {
  sync_grouping_mode(col, gmode);
  columns.emplace_back(std::move(col));
  names.emplace_back(std::move(name));
}


void Outputs::add(Column&& col, Grouping gmode) {
  sync_grouping_mode(col, gmode);
  columns.emplace_back(std::move(col));
  names.emplace_back(std::string());
}


// Add column df[i] to the outputs
//
void Outputs::add_column(size_t iframe, size_t icol) {
  const DataTable* df = ctx.get_datatable(iframe);
  const RowIndex& rowindex = ctx.get_rowindex(iframe);
  Column column { df->get_column(icol) };  // copy
  if (rowindex) {
    const RowIndex& ricol = column->rowindex();
    column->replace_rowindex(ctx._product(rowindex, ricol));
  }
  const std::string& name = df->get_names()[icol];
  // TODO: check whether the column belongs to the group key
  add(Column(column), std::string(name), Grouping::GtoALL);
}


void Outputs::append(Outputs&& other) {
  sync_grouping_mode(other);
  if (columns.size() == 0) {
    columns = std::move(other.columns);
    names = std::move(other.names);
  }
  else {
    for (auto& item : other.columns) {
      columns.emplace_back(std::move(item));
    }
    for (auto& item : other.names) {
      names.emplace_back(std::move(item));
    }
  }
}


void Outputs::apply_name(const std::string& name) {
  if (names.size() == 1) {
    names[0] = name;
  }
  else {
    for (auto& nameref : names) {
      if (nameref.empty()) {
        nameref = name;
      }
      else {
        // Note: name.c_str() returns pointer to a null-terminated character
        // array with the same data as `name`. The length of that array is
        // the length of `name` + 1 (for trailing NUL). We insert the entire
        // array into `item.name`, together with the NUL character, and then
        // overwrite the NUL with a '.'. The end result is that we have a
        // string with content "{name}.{item.name}".
        nameref.insert(0, name.c_str(), name.size() + 1);
        nameref[name.size()] = '.';
      }
    }
  }
}



size_t Outputs::size() const noexcept {
  return columns.size();
}

EvalContext& Outputs::get_workframe() const noexcept {
  return ctx;
}


Column& Outputs::get_column(size_t i) {
  return columns[i];
}

std::string& Outputs::get_name(size_t i) {
  return names[i];
}

Grouping Outputs::get_grouping_mode() const {
  return grouping_mode;
}



strvec& Outputs::get_names() {
  return names;
}


colvec& Outputs::get_columns() {
  return columns;
}




//------------------------------------------------------------------------------
// Grouping mode manipulation
//------------------------------------------------------------------------------

void Outputs::sync_grouping_mode(Outputs& other) {
  if (grouping_mode == other.grouping_mode) return;
  size_t g1 = static_cast<size_t>(grouping_mode);
  size_t g2 = static_cast<size_t>(other.grouping_mode);
  if (g1 < g2) increase_grouping_mode(other.grouping_mode);
  else         other.increase_grouping_mode(grouping_mode);
}


void Outputs::sync_grouping_mode(Column& col, Grouping gmode) {
  if (grouping_mode == gmode) return;
  size_t g1 = static_cast<size_t>(grouping_mode);
  size_t g2 = static_cast<size_t>(gmode);
  if (g1 < g2) increase_grouping_mode(gmode);
  else         column_increase_grouping_mode(col, gmode, grouping_mode);
}


void Outputs::increase_grouping_mode(Grouping gmode) {
  for (auto& col : columns) {
    column_increase_grouping_mode(col, grouping_mode, gmode);
  }
  grouping_mode = gmode;
}


void Outputs::column_increase_grouping_mode(
    Column& col, Grouping gfrom, Grouping gto)
{
  // TODO
  (void)col; (void) gfrom; (void) gto;
}




}}  // namespace dt::expr
