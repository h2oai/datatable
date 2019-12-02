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
#include <ctime>
#include <sstream>
#include "frame/repr/html_widget.h"
#include "python/string.h"
#include "utils/terminal/terminal.h"
#include "datatablemodule.h"
namespace dt {



static const char* imgx =
    "url('data:image/png;base64,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"
    "QSFHy1p4fgAAAAASUVORK5CYII=');";
static const char* imgv =
    "url('data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAABIAAAAkCAYAAACE7WrnAAA"
    "AdElEQVR42mP4wyMVQQ3M8P///whqYBSDkG2A8bGJo+tBMQifIbgMQ5ZjwGUIPjY2wxiwOZWQZ"
    "rxhhM0F6IYjq8PqNWyBh4+NN7CpGv2jBo0aNGrQqEGjBtHFIIoLf5pUR2RXkFStsqnSiKBqs4b"
    "i6KdW0w8AxFl+XL1lK8wAAAAASUVORK5CYII=');";



static py::oobj generate_stylesheet() {
  std::time_t now = std::time(nullptr);
  std::tm* t = std::localtime(&now);
  bool xd = (t->tm_mon == 11);
  bool vd = (t->tm_mon == 4) && (t->tm_wday == 4) &&
            (t->tm_mday >= 15) && (t->tm_mday <= 21);

  std::ostringstream html;
  html << "<style type='text/css'>\n";
  html << ".datatable table.frame { margin-bottom: 0; }\n"
          ".datatable table.frame thead { border-bottom: none; }\n"
          ".datatable table.frame tr.coltypes td {"
          "  color: #FFFFFF;"
          "  line-height: 6px;"
          "  padding: 0 0.5em;"
          "}\n"
          ".datatable .boolean { background: #DDDD99; }\n"
          ".datatable .object  { background: #565656; }\n"
          ".datatable .integer { background: #5D9E5D; }\n"
          ".datatable .float   { background: #4040CC; }\n"
          ".datatable .string  { background: #CC4040; }\n"
          ".datatable .row_index {"
          "  background: var(--jp-border-color3);"
          "  border-right: 1px solid var(--jp-border-color0);"
          "  color: var(--jp-ui-font-color3);"
          "  font-size: 9px;"
          "}\n"
          ".datatable .frame tr.coltypes .row_index {"
          "  background: var(--jp-border-color0);"
          "}\n"
          ".datatable th:nth-child(2) { padding-left: 12px; }\n"
          ".datatable .hellipsis {"
          "  color: var(--jp-cell-editor-border-color);"
          "}\n"
          ".datatable .vellipsis {"
          "  background: var(--jp-layout-color0);"
          "  color: var(--jp-cell-editor-border-color);"
          "}\n"
          ".datatable .na {"
          "  color: var(--jp-cell-editor-border-color);"
          "  font-size: 80%;"
          "}\n"
          ".datatable .footer { font-size: 9px; }\n"
          ".datatable .frame_dimensions {"
          "  background: var(--jp-border-color3);"
          "  border-top: 1px solid var(--jp-border-color0);"
          "  color: var(--jp-ui-font-color3);"
          "  display: inline-block;"
          "  opacity: 0.6;"
          "  padding: 1px 10px 1px 5px;"
          "}\n";
  if (xd || vd) {
    html << ".datatable .frame thead tr.colnames {"
            "  background-image: " << (xd? imgx : imgv) <<
            "  background-repeat: repeat-x;"
            "  background-size: 14px;"
            "  height: 28px;"
            "}\n";
  }
  html << "</style>\n";
  return py::ostring(html.str());
}


void emit_stylesheet() {
  auto html = generate_stylesheet();
  py::odict update_kwds;
  update_kwds.set(py::ostring("display_id"), py::ostring("datatable:css"));
  HtmlWidget::write_to_jupyter(html, update_kwds);
}




}  // namespace dt
namespace py {


static PKArgs args_init_styles(
  0, 0, 0, false, false, {}, "init_styles",
  "Inject datatable's stylesheets into the Jupyter notebook. This\n"
  "method does nothing when it runs in a normal Python environment\n"
  "outside of Jupyter.\n"
  "\n"
  "When datatable runs in a Jupyter notebook, it renders its Frames\n"
  "as HTML tables. The appearance of these tables is enhanced using\n"
  "a custom stylesheet, which must be injected into the notebook at\n"
  "any point on the page. This is exactly what this function does.\n"
  "\n"
  "Normally, this function is called automatically when datatable\n"
  "is imported. However, in some circumstances Jupyter erases these\n"
  "stylesheets (for example if you run ``import datatable`` cell\n"
  "twice). In such cases, you may need to call this method manually.\n"
);

static void init_styles(const PKArgs&) {
  dt::emit_stylesheet();
}

void DatatableModule::init_methods_styles() {
  ADD_FN(&init_styles, args_init_styles);
}




} // namespace py
