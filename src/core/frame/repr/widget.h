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
#include <vector>
#include "datatable.h"
#ifndef dt_FRAME_REPR_WIDGET_h
#define dt_FRAME_REPR_WIDGET_h
namespace dt {


/**
  * Base class for various widgets responsible for rendering a frame.
  * `TerminalWidget` outputs the frame into the terminal (text mode),
  * while `HtmlWidget` generates an HTML table, suitable for Jupyter
  * notebook.
  *
  * The widget normally represents only a subset of Frame's data. Two
  * modes are supported:
  *
  *   - "split view", indicated by `is_split_view_ == true`. In this
  *     mode we generate first `col0_` / last `col1_` columns, and
  *     first `rows0` / last `rows1` rows.
  *
  *   - "window view", indicated by `is_split_view_ == false`. In this
  *     mode we render a subrange of columns `[col0_; col1_)`, and a
  *     subrange of rows `[row0_; row1_)`.
  *
  * Note: we do not use inheritance to implement Split/Windowed view
  * because we want TerminalWidget / HtmlWidget to inherit from this
  * class.
  */
class Widget {
  private:
    size_t col0_, col1_;
    size_t row0_, row1_;
    bool is_split_view_;
    size_t : 56;

  protected:
    DataTable* dt_;
    std::vector<size_t> colindices_;
    std::vector<size_t> rowindices_;
    size_t ncols_, nrows_, nkeys_;
    bool render_row_indices_;
    size_t : 56;

  public:
    static constexpr size_t NA_index = size_t(-1);
    static struct SplitViewTag {} split_view_tag;
    static struct WindowedTag {} windowed_tag;

    Widget(DataTable* dt, SplitViewTag);
    Widget(DataTable* dt, WindowedTag);
    virtual ~Widget() = default;

    void render_all();

  private:
    explicit Widget(DataTable* dt);

    virtual void _render() = 0;
    void _generate_column_indices_split_view();
    void _generate_row_indices_split_view();
    void _generate_column_indices_windowed_view();
    void _generate_row_indices_windowed_view();
};




}  // namespace dt
#endif
