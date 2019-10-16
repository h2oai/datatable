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
#include "column/range.h"
#include "frame/repr/terminal_widget.h"
#include "python/string.h"
namespace dt {



TerminalWidget::TerminalWidget(DataTable* dt, Terminal* term, SplitViewTag)
  : Widget(dt, split_view_tag)
{
  TextColumn::setup(term);
}


py::oobj TerminalWidget::to_python() {
  render_all();
  const std::string outstr = out_.str();
  return py::ostring(outstr);
}




//------------------------------------------------------------------------------
// Rendering
//------------------------------------------------------------------------------

void TerminalWidget::_render() {
  _prerender_columns();
  _render_column_names();
  _render_header_separator();
  _render_data();
  _render_footer();
}


void TerminalWidget::_prerender_columns() {
  size_t nkeys = dt_->nkeys();
  size_t nrows = dt_->nrows();
  const auto& names = dt_->get_names();
  text_columns_.reserve(colindices_.size() + 1);
  if (nkeys == 0) {
    Column ri_col(new Range_ColumnImpl(0, static_cast<int64_t>(nrows), 1));
    TextColumn textcol("", ri_col, rowindices_, /*is_key_column=*/true);
    textcol.set_right_border();
    text_columns_.emplace_back(std::move(textcol));
  }
  for (size_t j : colindices_) {
    const auto& col = dt_->get_column(j);
    bool iskey = (j < nkeys);
    TextColumn textcol(names[j], col, rowindices_, iskey);
    if (j == nkeys - 1) textcol.set_right_border();
    text_columns_.emplace_back(std::move(textcol));
  }
  text_columns_.front().unset_left_margin();
  text_columns_.back().unset_right_margin();
}


void TerminalWidget::_render_column_names() {
  for (const auto& col : text_columns_) {
    col.print_name(out_);
  }
  out_ << '\n';
}


void TerminalWidget::_render_header_separator() {
  for (const auto& col : text_columns_) {
    col.print_separator(out_);
  }
  out_ << '\n';
}


void TerminalWidget::_render_data() {
  for (size_t k = 0; k < rowindices_.size(); ++k) {
    for (const auto& col : text_columns_) {
      col.print_value(out_, k);
    }
    out_ << '\n';
  }
}


void TerminalWidget::_render_footer() {
  size_t nrows = dt_->nrows();
  size_t ncols = dt_->ncols();
  out_ << '\n';
  out_ << "[" << nrows << " row" << (nrows==1? "" : "s") << " x ";
  out_ << ncols << " column" << (ncols==1? "" : "s") << "]";
  out_ << '\n';
}




}  // namespace dt
