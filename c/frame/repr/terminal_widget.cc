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
#include "utils/c+++.h"
namespace dt {



TerminalWidget::TerminalWidget(DataTable* dt, Terminal* term, SplitViewTag)
  : Widget(dt, split_view_tag),
    out_(term->colors_enabled())
{
  has_rowindex_column_ = false;
  terminal_ = term;
  TextColumn::setup(term);
}


py::oobj TerminalWidget::to_python() {
  render_all();
  const std::string outstr = out_.str();
  return py::ostring(outstr);
}

void TerminalWidget::to_stdout() {
  // In IPython, we insert an extra newline in front, because IPython
  // prints "Out [X]: " in front of the output value, which causes all
  // column headers to become misaligned.
  // Likewise, IPython tends to insert an extra newline at the end of
  // the output, so we remove our own extra newline.
  if (terminal_->is_ipython()) out_ << '\n';
  render_all();
  std::string outstr = out_.str();
  if (terminal_->is_ipython() && outstr.back() == '\n') {
    outstr.back() = '\0';
  }
  py::write_to_stdout(outstr);
}



//------------------------------------------------------------------------------
// Rendering
//------------------------------------------------------------------------------

void TerminalWidget::_render() {
  const TerminalSize ts = terminal_->get_size();
  _prerender_columns(ts.width);
  _render_column_names();
  _render_header_separator();
  _render_data();
  _render_footer();
}


void TerminalWidget::_prerender_columns(int terminal_width)
{
  // +2 because we'll be removing left+right margins in the end
  int remaining_width = terminal_width + 2;
  size_t nkeys = dt_->nkeys();
  const auto& names = dt_->get_names();

  // +2 for row indices and the vertical separator
  text_columns_.resize(colindices_.size() + 2);

  // how many extra columns were added into `text_columns_`
  size_t k0 = 0;

  // If there are no keys, we add a "row numbers" column + a vertical
  // separator column.
  if (nkeys == 0) {
    auto irows = static_cast<int64_t>(dt_->nrows());
    text_columns_[0] = text_column(
      new Data_TextColumn("",
                          Column(new Range_ColumnImpl(0, irows, 1)),
                          rowindices_,
                          remaining_width)
    );
    text_columns_[1] = text_column(new VSep_TextColumn());
    remaining_width -= text_columns_[0]->get_width();
    remaining_width -= text_columns_[1]->get_width();
    has_rowindex_column_ = true;
    k0 = 2;
  }

  // Render all other columns in the order of priority
  auto order = _order_colindices();
  bool make_ellipsis_column = false;
  for (size_t i : order) {
    size_t j = colindices_[i];  // column index within `dt_`
    size_t k = i + k0;          // column index within `text_columns_`
    xassert(!text_columns_[k]);

    if (j == NA_index) {
      make_ellipsis_column = true;
    }
    else if (i == order.back()) {
      // Min.width required to render a data column is 4. If we are
      // on the last column to render, but there's not enough
      // remaining width, then render the ellipsis column instead.
      if (remaining_width <= 4) make_ellipsis_column = true;
    }
    else {
      // If this is not the last column, then in order for it to be
      // rendered, there must be enough available width for this
      // column (4), and for an ellipsis column (3).
      if (remaining_width <= 4 + 3) make_ellipsis_column = true;
    }

    if (make_ellipsis_column) {
      text_columns_[k] = text_column(new Ellipsis_TextColumn());
      break;
    }

    int target_width = remaining_width - 3 * (i != order.back());
    text_columns_[k] = text_column(new Data_TextColumn(names[j],
                                                       dt_->get_column(j),
                                                       rowindices_,
                                                       target_width));
    remaining_width -= text_columns_[k]->get_width();

    if (nkeys && j == nkeys-1) {
      // NB: cannot use .cbegin() here because of gcc4.8
      text_columns_.insert(text_columns_.begin() + static_cast<long>(k) + 1,
                           text_column(new VSep_TextColumn()));
      k0++;
      remaining_width -= text_columns_[k+1]->get_width();
    }
  }

  // Remove all empty columns from `text_columns_`
  size_t j = 0;
  for (size_t i = 0; i < text_columns_.size(); ++i) {
    if (!text_columns_[i]) continue;
    if (i != j) {
      std::swap(text_columns_[j], text_columns_[i]);
    }
    ++j;
  }
  text_columns_.resize(j);
  text_columns_.front()->unset_left_margin();
  text_columns_.back()->unset_right_margin();
}


/**
  * Establish the order in which the columns in `colindices_` has to
  * be rendered. This is a helper function for `_prerender_columns()`.
  *
  * Generally, the `colindices_` have the following structure:
  *
  *   i0, i1, i2, ... ik, <...>, ikk, ..., in
  *   [    left_cols    ]      [ right_cols ]
  *
  * The "ellipsis" column may or may not be present. If it is present,
  * we call all columns before the ellipsis "left columns", and all
  * columns after are "right columns". If there is no ellipsis column
  * then all columns are considered "left".
  *
  * When the columns will be rendered we want to prioritze them in
  * such order that:
  *
  *   - first we render the key columns, if any (they will always be
  *     at the start of colindices);
  *
  *   - then we alternate rendering columns from the left and from
  *     the right, with columns further away from the place of
  *     ellipsis having more priority;
  *
  *   - the order in which columns from the left and from the right
  *     are taken is such the taken and the remaining columns on both
  *     sides are roughly proportional to their initial counts.
  *
  *   - the ellipsis column is rendered lsat, if present.
  *
  */
std::vector<size_t> TerminalWidget::_order_colindices() const {
  size_t nkeys  = dt_->nkeys();
  size_t n = colindices_.size();
  std::vector<size_t> order;
  order.reserve(n);

  size_t i = 0;
  for (; i < n && colindices_[i] < nkeys; i++) {}
  size_t ncols_key = i;
  for (; i < n && colindices_[i] != NA_index; i++) {}
  size_t i_ellipsis = i;
  size_t ncols_left = i - ncols_key;
  size_t ncols_right = i < n? n - i - 1 : 0;

  for (i = 0; i < ncols_key; ++i) order.push_back(i);
  size_t weight_left = 0;
  size_t weight_right = 0;
  size_t ileft = ncols_key;
  size_t iright = ncols_right? n - 1 : i_ellipsis;
  while (true) {
    bool has_left = (ileft != i_ellipsis);
    bool has_right = (iright != i_ellipsis);
    if (has_left && (weight_left <= weight_right || !has_right)) {
      order.push_back(ileft++);
      weight_left += ncols_right;
    } else if (has_right) {
      order.push_back(iright--);
      weight_right += ncols_left;
    } else {
      xassert(!has_left && !has_right);
      if (i_ellipsis < n) order.push_back(i_ellipsis);
      break;
    }
  }
  return order;
}


void TerminalWidget::_render_column_names() {
  out_ << style::bold;
  for (const auto& col : text_columns_) {
    col->print_name(out_);
  }
  out_ << style::end;
  out_ << '\n';
}


void TerminalWidget::_render_header_separator() {
  out_ << style::grey;
  for (const auto& col : text_columns_) {
    col->print_separator(out_);
  }
  out_ << style::end;
  out_ << '\n';
}


void TerminalWidget::_render_data() {
  for (size_t k = 0; k < rowindices_.size(); ++k) {
    if (has_rowindex_column_) {
      out_ << style::grey;
      text_columns_[0]->print_value(out_, k);
      out_ << style::end;
    }
    for (size_t i = has_rowindex_column_; i < text_columns_.size(); ++i) {
      text_columns_[i]->print_value(out_, k);
    }
    out_ << '\n';
  }
}


void TerminalWidget::_render_footer() {
  size_t nrows = dt_->nrows();
  size_t ncols = dt_->ncols();
  out_ << '\n';
  out_ << style::dim;
  out_ << "[" << nrows << " row" << (nrows==1? "" : "s") << " x ";
  out_ << ncols << " column" << (ncols==1? "" : "s") << "]";
  out_ << style::end;
  out_ << '\n';
}




}  // namespace dt
