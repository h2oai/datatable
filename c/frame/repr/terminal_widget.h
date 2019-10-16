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
#include <vector>
#include "frame/repr/text_column.h"
#include "frame/repr/widget.h"
#include "python/_all.h"
namespace dt {


/**
  * This class is responsible for rendering a Frame into a terminal
  * as a text.
  */
class TerminalWidget : public Widget {
  private:
    std::ostringstream out_;
    std::vector<TextColumn> text_columns_;

  public:
    TerminalWidget(DataTable* dt, SplitViewTag);
    py::oobj to_python();

  protected:
    void _render() override;

  private:
    void _prerender_columns();
    void _render_column_names();
    void _render_header_separator();
    void _render_data();
    void _render_footer();
};




}  // namespace dt
#endif
