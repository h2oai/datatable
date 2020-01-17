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
#ifndef dt_FRAME_REPR_TERMINAL_WIDGET_h
#define dt_FRAME_REPR_TERMINAL_WIDGET_h
#include <iostream>
#include <memory>
#include <vector>
#include "frame/repr/text_column.h"
#include "frame/repr/widget.h"
#include "python/_all.h"
#include "utils/terminal/terminal_stream.h"
namespace dt {

using text_column = std::unique_ptr<TextColumn>;


/**
  * This class is responsible for rendering a Frame into a terminal
  * as a text.
  */
class TerminalWidget : public Widget {
  private:
    TerminalStream out_;
    std::vector<text_column> text_columns_;
    dt::Terminal* terminal_;
    bool has_rowindex_column_;
    size_t : 56;

  public:
    TerminalWidget(DataTable* dt, Terminal* term, SplitViewTag);

    py::oobj to_python();
    void     to_stdout();

  protected:
    void _render() override;

  private:
    void _prerender_columns(int terminal_width);
    std::vector<size_t> _order_colindices() const;

    void _render_column_names();
    void _render_header_separator();
    void _render_data();
    void _render_footer();
};




}  // namespace dt
#endif
