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
#include "expr/workframe.h"
#include "column_impl.h"
#include "datatable.h"
namespace dt {
namespace expr {


//------------------------------------------------------------------------------
// Workframe::Record
//------------------------------------------------------------------------------

Workframe::Record::Record()
  : frame_id(INVALID_FRAME) {}

Workframe::Record::Record(Column&& col, std::string&& name_)
  : column(std::move(col)),
    name(std::move(name_)),
    frame_id(INVALID_FRAME) {}

Workframe::Record::Record(Column&& col, const std::string& name_, size_t fid,
                          size_t cid)
  : column(std::move(col)),
    name(name_),
    frame_id(static_cast<uint32_t>(fid)),
    column_id(static_cast<uint32_t>(cid)) {}




//------------------------------------------------------------------------------
// Workframe
//------------------------------------------------------------------------------

Workframe::Workframe(EvalContext& ctx_)
  : ctx(ctx_),
    grouping_mode(Grouping::SCALAR) {}



void Workframe::add_column(Column&& col, std::string&& name, Grouping gmode) {
  sync_grouping_mode(col, gmode);
  entries.emplace_back(std::move(col), std::move(name));
}


void Workframe::add_ref_column(size_t iframe, size_t icol) {
  const DataTable* df = ctx.get_datatable(iframe);
  const RowIndex& rowindex = ctx.get_rowindex(iframe);
  Column column { df->get_column(icol) };  // copy
  if (rowindex) {
    const RowIndex& ricol = column->rowindex();
    column->replace_rowindex(ctx._product(rowindex, ricol));
  }
  const std::string& name = df->get_names()[icol];

  auto gmode = (grouping_mode <= Grouping::GtoONE &&
                iframe == 0 &&
                ctx.has_groupby() &&
                ctx.get_by_node().has_group_column(icol)) ? Grouping::GtoONE
                                                          : Grouping::GtoALL;
  sync_grouping_mode(column, gmode);
  entries.emplace_back(std::move(column), name, iframe, icol);
}


void Workframe::add_placeholder(const std::string& name, size_t iframe) {
  entries.emplace_back(Column(), std::string(name), iframe, 0);
}


void Workframe::cbind(Workframe&& other) {
  sync_grouping_mode(other);
  if (entries.size() == 0) {
    entries = std::move(other.entries);
  }
  else {
    for (auto& item : other.entries) {
      entries.emplace_back(std::move(item));
    }
  }
}


void Workframe::rename(const std::string& newname) {
  if (entries.size() == 1) {
    entries[0].name = newname;
  }
  else {
    size_t len = newname.size();
    for (auto& info : entries) {
      if (info.name.empty()) {
        info.name = newname;
      }
      else {
        // Note: name.c_str() returns pointer to a null-terminated character
        // array with the same data as `name`. The length of that array is
        // the length of `name` + 1 (for trailing NUL). We insert the entire
        // array into `item.name`, together with the NUL character, and then
        // overwrite the NUL with a '.'. The end result is that we have a
        // string with content "{newname}.{item.name}".
        info.name.insert(0, newname.c_str(), len + 1);
        info.name[len] = '.';
      }
    }
  }
}



size_t Workframe::ncols() const noexcept {
  return entries.size();
}

size_t Workframe::nrows() const noexcept {
  return entries.empty()? 0 : entries[0].column.nrows();
}

EvalContext& Workframe::get_context() const noexcept {
  return ctx;
}

bool Workframe::is_computed_column(size_t i) const {
  return entries[i].frame_id == Record::INVALID_FRAME;
}

bool Workframe::is_placeholder_column(size_t i) const {
  return !entries[i].column;
}

bool Workframe::is_reference_column(
    size_t i, size_t* iframe, size_t* icol) const
{
  xassert(is_computed_column(i) + is_placeholder_column(i) <= 1);
  *iframe = entries[i].frame_id;
  *icol  = entries[i].column_id;
  return !(is_computed_column(i) || is_placeholder_column(i));
}


// Ensure that this workframe is suitable for updating a region
// of the requested shape [target_nrows x target_ncols].
//
void Workframe::reshape_for_update(size_t target_nrows, size_t target_ncols) {
  size_t this_nrows = nrows();
  size_t this_ncols = ncols();
  if (this_ncols == 0 && target_ncols == 0 && this_nrows == 0) return;
  bool ok = (this_nrows == target_nrows || this_nrows == 1) &&
            (this_ncols == target_ncols || this_ncols == 1);
  if (!ok) {
    throw ValueError() << "Invalid replacement Frame: expected ["
        << target_nrows << " x " << target_ncols << "], but received ["
        << this_nrows << " x " << this_ncols << "]";
  }
  if (this_nrows != target_nrows) {
    xassert(this_nrows == 1);
    for (auto& item : entries) {
      item.column.repeat(target_nrows);  // modifies the column in-place
    }
  }
  if (this_ncols != target_ncols) {
    xassert(this_ncols == 1);
    entries.resize(target_ncols, entries[0]);
  }
  xassert(nrows() == target_nrows);
  xassert(ncols() == target_ncols);
}


const Column& Workframe::get_column(size_t i) const {
  return entries[i].column;
}


std::string Workframe::retrieve_name(size_t i) {
  xassert(i < entries.size());
  return std::move(entries[i].name);
}


Column Workframe::retrieve_column(size_t i) {
  xassert(i < entries.size());
  return std::move(entries[i].column);
}


void Workframe::replace_column(size_t i, Column&& col) {
  xassert(i < entries.size());
  xassert(!entries[i].column);
  entries[i].column = std::move(col);
  entries[i].frame_id = Record::INVALID_FRAME;
}


Grouping Workframe::get_grouping_mode() const {
  return grouping_mode;
}



std::unique_ptr<DataTable> Workframe::convert_to_datatable() && {
  colvec columns;
  strvec names;
  names.reserve(entries.size());
  columns.reserve(entries.size());
  for (auto& record : entries) {
    columns.emplace_back(std::move(record.column));
    names.emplace_back(std::move(record.name));
  }
  return dtptr(new DataTable(std::move(columns), std::move(names), false));
}





//------------------------------------------------------------------------------
// Grouping mode manipulation
//------------------------------------------------------------------------------

void Workframe::sync_grouping_mode(Workframe& other) {
  if (grouping_mode == other.grouping_mode) return;
  size_t g1 = static_cast<size_t>(grouping_mode);
  size_t g2 = static_cast<size_t>(other.grouping_mode);
  if (g1 < g2) increase_grouping_mode(other.grouping_mode);
  else         other.increase_grouping_mode(grouping_mode);
}


void Workframe::sync_grouping_mode(Column& col, Grouping gmode) {
  if (grouping_mode == gmode) return;
  size_t g1 = static_cast<size_t>(grouping_mode);
  size_t g2 = static_cast<size_t>(gmode);
  if (g1 < g2) increase_grouping_mode(gmode);
  else         column_increase_grouping_mode(col, gmode, grouping_mode);
}


void Workframe::increase_grouping_mode(Grouping gmode) {
  for (auto& item : entries) {
    column_increase_grouping_mode(item.column, grouping_mode, gmode);
  }
  grouping_mode = gmode;
}


void Workframe::column_increase_grouping_mode(
    Column& col, Grouping gfrom, Grouping gto)
{
  // TODO
  (void)col; (void) gfrom; (void) gto;
}




}}  // namespace dt::expr
