//------------------------------------------------------------------------------
// Copyright 2019-2021 H2O.ai
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
#include <algorithm>                  // std::max
#include <string>
#include "frame/repr/repr_options.h"
#include "frame/repr/widget.h"
namespace dt {

constexpr size_t Widget::NA_index;
Widget::SplitViewTag Widget::split_view_tag;
Widget::WindowedTag Widget::windowed_tag;



//------------------------------------------------------------------------------
// Construction
//------------------------------------------------------------------------------

Widget::Widget(DataTable* dt) {
  dt_ = dt;
  ncols_ = dt->ncols();
  nrows_ = dt->nrows();
  nkeys_ = dt->nkeys();
  render_row_indices_ = (nkeys_ == 0);
}


Widget::Widget(DataTable* dt, SplitViewTag) : Widget(dt)
{
  is_split_view_ = true;

  constexpr size_t maxcols = 15;
  col0_ = (ncols_ <= maxcols) ? ncols_ : maxcols * 2 / 3;
  col1_ = (ncols_ <= maxcols) ? 0 : maxcols - col0_;
  col0_ = std::max(col0_, dt->nkeys());

  size_t max_nrows = std::max(display_max_nrows,
                              display_head_nrows + display_tail_nrows);
  row0_ = (nrows_ > max_nrows) ? display_head_nrows : nrows_;
  row1_ = (nrows_ > max_nrows) ? display_tail_nrows : 0;
}


Widget::Widget(DataTable* dt, WindowedTag) : Widget(dt)
{
  is_split_view_ = false;
  col0_ = nkeys_;
  col1_ = 15;
  row0_ = 0;
  row1_ = 30;
}




//------------------------------------------------------------------------------
// Rendering
//------------------------------------------------------------------------------

void Widget::render_all() {
  if (is_split_view_) {
    _generate_column_indices_split_view();
    _generate_row_indices_split_view();
  } else {
    _generate_column_indices_windowed_view();
    _generate_row_indices_windowed_view();
  }
  _render();
}


void Widget::_generate_column_indices_split_view() {
  colindices_.reserve(col0_ + col1_ + 1);
  for (size_t i = 0; i < ncols_; ++i) {
    if (i == col0_) {
      colindices_.push_back(NA_index);
      if (col1_ == 0) break;
      i = ncols_ - col1_;
    }
    colindices_.push_back(i);
  }
}


void Widget::_generate_column_indices_windowed_view() {
  colindices_.reserve(nkeys_ + col0_);
  for (size_t i = 0; i < nkeys_; ++i) {
    colindices_.push_back(i);
  }
  for (size_t i = col0_; i < col1_; ++i) {
    colindices_.push_back(i);
  }
}


// Populate array `rowindices_` with indices of the rows that shall be
// rendered. The array may also contain `NA_index`, which indicates an
// "ellipsis" row.
void Widget::_generate_row_indices_split_view() {
  rowindices_.reserve(row0_ + row1_ + 1);
  for (size_t i = 0; i < nrows_; ++i) {
    if (i == row0_) {
      rowindices_.push_back(NA_index);
      if (row1_ == 0) break;
      i = nrows_ - row1_;
    }
    rowindices_.push_back(i);
  }
}


void Widget::_generate_row_indices_windowed_view() {
  rowindices_.reserve(row0_ + 2);
  if (row0_ > 0) rowindices_.push_back(NA_index);
  for (size_t i = row0_; i < row1_; ++i) {
    rowindices_.push_back(i);
  }
  if (row1_ < nrows_) rowindices_.push_back(NA_index);
}




}  // namespace dt
