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
  startcol_ = NA_index;
  startrow_ = NA_index;

  constexpr size_t maxcols = 15;
  cols0_ = (ncols_ <= maxcols) ? ncols_ : maxcols * 2 / 3;
  cols1_ = (ncols_ <= maxcols) ? 0 : maxcols - cols0_;
  cols0_ = std::max(cols0_, dt->nkeys());

  size_t max_nrows = std::max(display_max_nrows,
                              display_head_nrows + display_tail_nrows);
  rows0_ = (nrows_ > max_nrows) ? display_head_nrows : nrows_;
  rows1_ = (nrows_ > max_nrows) ? display_tail_nrows : 0;
}


Widget::Widget(DataTable* dt, WindowedTag) : Widget(dt)
{
  startcol_ = 0;
  startrow_ = 0;
  cols0_ = 15;
  rows0_ = 30;
}




//------------------------------------------------------------------------------
// Rendering
//------------------------------------------------------------------------------

void Widget::render_all() {
  _generate_column_indices();
  _generate_row_indices();
  _render();
}


void Widget::_generate_column_indices() {
  colindices_.clear();

  // Split view mode
  if (startcol_ == NA_index) {
    colindices_.reserve(cols0_ + cols1_ + 1);
    for (size_t i = 0; i < ncols_; ++i) {
      if (i == cols0_) {
        colindices_.push_back(NA_index);
        if (cols1_ == 0) break;
        i = ncols_ - cols1_;
      }
      colindices_.push_back(i);
    }
  }
  // Windowed mode
  else {
    colindices_.reserve(nkeys_ + cols0_);
    for (size_t i = 0; i < nkeys_; ++i) {
      colindices_.push_back(i);
    }
    for (size_t i = 0; i < cols0_; ++i) {
      colindices_.push_back(i + startcol_);
    }
  }
}


// Populate array `rowindices_` with indices of the rows that shall be
// rendered. The array may also contain `NA_index`, which indicates an
// "ellipsis" row.
void Widget::_generate_row_indices() {
  rowindices_.clear();

  // Split view mode
  if (startrow_ == NA_index) {
    rowindices_.reserve(rows0_ + rows1_ + 1);
    for (size_t i = 0; i < nrows_; ++i) {
      if (i == rows0_) {
        rowindices_.push_back(NA_index);
        if (rows1_ == 0) break;
        i = nrows_ - rows1_;
      }
      rowindices_.push_back(i);
    }
  }
  // Windowed mode
  else {
    rowindices_.reserve(rows0_ + 2);
    if (startrow_ > 0) rowindices_.push_back(NA_index);
    for (size_t i = 0; i < rows0_; ++i) {
      rowindices_.push_back(i + startrow_);
    }
    if (rows0_ + startrow_ < nrows_) rowindices_.push_back(NA_index);
  }
}




}  // namespace dt
