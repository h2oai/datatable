//------------------------------------------------------------------------------
// Copyright 2018-2019 H2O.ai
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
#include <sstream>
#include "frame/py_frame.h"
#include "frame/repr/repr_options.h"
#include "frame/repr/terminal_widget.h"
#include "frame/repr/text_column.h"
#include "frame/repr/widget.h"
#include "frame/repr/html_widget.h"
#include "python/string.h"
#include "options.h"
#include "types.h"


//------------------------------------------------------------------------------
// py::Frame interface
//------------------------------------------------------------------------------
namespace py {


oobj Frame::m__repr__() const {
  size_t nrows = dt->nrows();
  size_t ncols = dt->ncols();
  std::ostringstream out;
  out << "<Frame [" << nrows << " row" << (nrows == 1? "" : "s")
      << " x " << ncols << " col" << (ncols == 1? "" : "s") << "]>";
  return ostring(out.str());
}


oobj Frame::m__str__() const {
  dt::TerminalWidget widget(dt,
                            &dt::Terminal::plain_terminal(),
                            dt::Widget::split_view_tag);
  return widget.to_python();
}


static PKArgs args__repr_html_(
  0, 0, 0, false, false, {}, "_repr_html_", nullptr);

oobj Frame::_repr_html_(const PKArgs&) {
  dt::HtmlWidget widget(dt);
  return widget.to_python();
}


static PKArgs args__repr_pretty_(
    0, 2, 0, false, false, {"p", "cycle"}, "_repr_pretty_", nullptr);

oobj Frame::_repr_pretty_(const PKArgs&) {
  xassert(!dt::Terminal::standard_terminal().is_jupyter());
  return oobj(this).invoke("view", obool(false));
}


static PKArgs args_view(
  0, 2, 0, false, false, {"interactive", "plain"}, "view", nullptr);

void Frame::view(const PKArgs& args) {
  bool interactive = true;  // default when `interactive` is omitted entirely
  bool is_jupyter = dt::Terminal::standard_terminal().is_jupyter();
  bool plain = args[1].to<bool>(false);
  if (args[0].is_none()) interactive = dt::display_interactive;
  if (args[0].is_bool()) interactive = args[0].to_bool_strict();

  if (is_jupyter) {
    auto htmlstr = _repr_html_(args__repr_html_);
    dt::HtmlWidget::write_to_jupyter(htmlstr);
  }
  else if (interactive) {
    oobj DFWidget = oobj::import("datatable.widget", "DataFrameWidget");
    DFWidget.call({oobj(this), obool(interactive)}).invoke("render");
  } else {
    auto terminal = plain? &dt::Terminal::plain_terminal()
                         : &dt::Terminal::standard_terminal();
    dt::TerminalWidget widget(dt, terminal, dt::Widget::split_view_tag);
    widget.to_stdout();
  }
}




void Frame::_init_repr(XTypeMaker& xt) {
  xt.add(METHOD__REPR__(&Frame::m__repr__));
  xt.add(METHOD__STR__(&Frame::m__str__));
  xt.add(METHOD(&Frame::_repr_html_, args__repr_html_));

  // Jupyter may call this method at a wrong time, so just do not create it
  // when datatable is running from a Jupyter notebook
  if (!dt::Terminal::standard_terminal().is_jupyter()) {
    xt.add(METHOD(&Frame::_repr_pretty_, args__repr_pretty_));
  }

  xt.add(METHOD(&Frame::view, args_view));
}


}  // namespace py
