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
#include "expr/eval_context.h"
#include "expr/workframe.h"
#include "datatable.h"
namespace dt {
namespace expr {


//------------------------------------------------------------------------------
// Workframe::Record (helper struct)
//------------------------------------------------------------------------------

Workframe::Record::Record()
  : frame_id(INVALID_FRAME) {}

Workframe::Record::Record(Column&& col, std::string&& name_)
  : column(std::move(col)),
    name(std::move(name_)),
    frame_id(INVALID_FRAME) {}

Workframe::Record::Record(Column&& col, const std::string& name_,
                          size_t fid, size_t cid)
  : column(std::move(col)),
    name(name_),
    frame_id(static_cast<uint32_t>(fid)),
    column_id(static_cast<uint32_t>(cid)) {}




//------------------------------------------------------------------------------
// Workframe
//------------------------------------------------------------------------------

Workframe::Workframe(EvalContext& ctx)
  : ctx_(ctx),
    grouping_mode_(Grouping::SCALAR) {}



void Workframe::add_column(Column&& col, std::string&& name, Grouping gmode) {
  sync_grouping_mode(col, gmode);
  entries_.emplace_back(std::move(col), std::move(name));
}


void Workframe::add_ref_column(size_t ifr, size_t icol) {
  const DataTable* df = ctx_.get_datatable(ifr);
  const RowIndex& rowindex = ctx_.get_rowindex(ifr);
  Column column = df->get_column(icol);  // copy
  column.apply_rowindex(rowindex);
  const std::string& name = df->get_names()[icol];

  // Detect whether the column participates in a groupby
  Grouping gmode = Grouping::GtoALL;
  if (grouping_mode_ <= Grouping::GtoONE && ctx_.has_group_column(ifr, icol)) {
    gmode = Grouping::GtoONE;
    column.apply_rowindex(ctx_.get_group_rowindex());
  }
  sync_grouping_mode(column, gmode);
  entries_.emplace_back(std::move(column), name, ifr, icol);
}


void Workframe::add_placeholder(const std::string& name, size_t ifr) {
  xassert(ifr != Record::INVALID_FRAME);
  entries_.emplace_back(Column(), std::string(name), ifr, 0);
}


void Workframe::cbind(Workframe&& other, bool at_end) {
  sync_grouping_mode(other);
  if (at_end && !entries_.empty()) {
    entries_.reserve(entries_.size() + other.entries_.size());
    for (auto& item : other.entries_) {
      entries_.emplace_back(std::move(item));
    }
  }
  else {
    other.entries_.reserve(entries_.size() + other.entries_.size());
    for (auto& item : entries_) {
      other.entries_.emplace_back(std::move(item));
    }
    entries_ = std::move(other.entries_);
  }
}


void Workframe::remove(const Workframe& other) {
  constexpr uint32_t DELETED_COLUMN = Record::INVALID_FRAME - 1;
  for (const auto& entry : other.entries_) {
    if (entry.frame_id == Record::INVALID_FRAME) {
      throw TypeError() << "Computed columns cannot be used in `.remove()`";
    }
    if (entry.column) {  // "Reference" column
      auto frid = entry.frame_id;
      auto colid = entry.column_id;
      for (auto& e : entries_) {
        if (e.frame_id == frid && e.column_id == colid) {
          e.frame_id = DELETED_COLUMN;
          break;
        }
      }
    }
    else {  // "Placeholder" column
      const auto& name = entry.name;
      for (auto& e : entries_) {
        if (!e.column && e.name == name) {
          e.frame_id = DELETED_COLUMN;
          break;
        }
      }
    }
  }
  // Clean up deleted columns
  size_t j = 0;
  for (size_t i = 0; i < entries_.size(); ++i) {
    if (entries_[i].frame_id == DELETED_COLUMN) continue;
    if (i != j) entries_[j] = std::move(entries_[i]);
    j++;
  }
  entries_.resize(j);
}


void Workframe::rename(const std::string& newname) {
  if (entries_.size() == 1) {
    entries_[0].name = newname;
  }
  else {
    size_t len = newname.size();
    for (auto& info : entries_) {
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
  return entries_.size();
}

size_t Workframe::nrows() const noexcept {
  if (entries_.empty()) return 0;
  if (!entries_[0].column) return 0;
  return entries_[0].column.nrows();
}

EvalContext& Workframe::get_context() const noexcept {
  return ctx_;
}

bool Workframe::is_computed_column(size_t i) const {
  return entries_[i].frame_id == Record::INVALID_FRAME;
}

bool Workframe::is_placeholder_column(size_t i) const {
  return !entries_[i].column;
}

bool Workframe::is_reference_column(
    size_t i, size_t* iframe, size_t* icol) const
{
  xassert(is_computed_column(i) + is_placeholder_column(i) <= 1);
  *iframe = entries_[i].frame_id;
  *icol  = entries_[i].column_id;
  return !(is_computed_column(i) || is_placeholder_column(i));
}


void Workframe::repeat_column(size_t n) {
  xassert(ncols() == 1);
  if (n == 1) return;
  entries_.resize(n, entries_[0]);
}


void Workframe::truncate_columns(size_t n) {
  xassert(ncols() >= n);
  entries_.resize(n);
}


// Ensure that this workframe is suitable for updating a region
// of the requested shape [target_nrows x target_ncols].
//
void Workframe::reshape_for_update(size_t target_nrows, size_t target_ncols) {
  size_t this_nrows = nrows();
  size_t this_ncols = ncols();
  if (this_ncols == 0 && target_ncols == 0 && this_nrows == 0) return;
  if (grouping_mode_ != Grouping::GtoALL) {
    increase_grouping_mode(Grouping::GtoALL);
    this_nrows = nrows();
  }
  bool ok = (this_nrows == target_nrows) &&
            (this_ncols == target_ncols || this_ncols == 1);
  if (!ok) {
    throw ValueError() << "Invalid replacement Frame: expected ["
        << target_nrows << " x " << target_ncols << "], but received ["
        << this_nrows << " x " << this_ncols << "]";
  }
  if (this_ncols != target_ncols) {
    xassert(this_ncols == 1);
    entries_.resize(target_ncols, entries_[0]);
  }
  xassert(nrows() == target_nrows);
  xassert(ncols() == target_ncols);
}


const Column& Workframe::get_column(size_t i) const {
  return entries_[i].column;
}


std::string Workframe::retrieve_name(size_t i) {
  xassert(i < entries_.size());
  return std::move(entries_[i].name);
}


Column Workframe::retrieve_column(size_t i) {
  xassert(i < entries_.size());
  return std::move(entries_[i].column);
}


void Workframe::replace_column(size_t i, Column&& col) {
  xassert(i < entries_.size());
  xassert(!entries_[i].column);
  entries_[i].column = std::move(col);
  entries_[i].frame_id = Record::INVALID_FRAME;
}


Grouping Workframe::get_grouping_mode() const {
  return grouping_mode_;
}



std::unique_ptr<DataTable> Workframe::convert_to_datatable() && {
  colvec columns;
  strvec names;
  names.reserve(entries_.size());
  columns.reserve(entries_.size());
  for (auto& record : entries_) {
    columns.emplace_back(std::move(record.column));
    names.emplace_back(std::move(record.name));
  }
  return std::unique_ptr<DataTable>(
            new DataTable(std::move(columns), std::move(names), false));
}





//------------------------------------------------------------------------------
// Grouping mode manipulation
//------------------------------------------------------------------------------

void Workframe::sync_grouping_mode(Workframe& other) {
  if (grouping_mode_ != other.grouping_mode_) {
    size_t g1 = static_cast<size_t>(grouping_mode_);
    size_t g2 = static_cast<size_t>(other.grouping_mode_);
    if (g1 < g2) increase_grouping_mode(other.grouping_mode_);
    else         other.increase_grouping_mode(grouping_mode_);
  }
  // xassert(ncols() == 0 || other.ncols() == 0 || nrows() == other.nrows());
}


void Workframe::sync_grouping_mode(Column& col, Grouping gmode) {
  if (grouping_mode_ != gmode) {
    size_t g1 = static_cast<size_t>(grouping_mode_);
    size_t g2 = static_cast<size_t>(gmode);
    if (g1 < g2) increase_grouping_mode(gmode);
    else         column_increase_grouping_mode(col, gmode, grouping_mode_);
  }
  xassert(ncols() == 0 || nrows() == col.nrows());
}


void Workframe::increase_grouping_mode(Grouping gmode) {
  if (grouping_mode_ == gmode) return;
  for (auto& item : entries_) {
    if (!item.column) continue;  // placeholder column
    column_increase_grouping_mode(item.column, grouping_mode_, gmode);
  }
  grouping_mode_ = gmode;
}


void Workframe::column_increase_grouping_mode(
    Column& col, Grouping gfrom, Grouping gto)
{
  xassert(gfrom != Grouping::GtoFEW && gfrom != Grouping::GtoANY);
  xassert(gto != Grouping::GtoFEW && gto != Grouping::GtoANY);
  xassert(static_cast<int>(gfrom) < static_cast<int>(gto));
  if (gfrom == Grouping::SCALAR && gto == Grouping::GtoONE) {
    col.repeat(ctx_.get_groupby().size());
  }
  else if (gfrom == Grouping::SCALAR && gto == Grouping::GtoALL) {
    col.repeat(ctx_.nrows());
  }
  else if (gfrom == Grouping::GtoONE && gto == Grouping::GtoALL) {
    if (col.is_constant()) {
      col.resize(1);
      col.repeat(ctx_.nrows());
    } else {
      col.apply_rowindex(ctx_.get_ungroup_rowindex());
    }
    xassert(col.nrows() == ctx_.nrows());
  }
  else {
    throw RuntimeError() << "Unexpected Grouping mode";  // LCOV_EXCL_LINE
  }
}




}}  // namespace dt::expr
