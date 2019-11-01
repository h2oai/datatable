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
#include "python/string.h"
#include "options.h"
#include "types.h"




//------------------------------------------------------------------------------
// HtmlWidget
//------------------------------------------------------------------------------

/**
  * This class is responsible for rendering a Frame into HTML.
  */
class HtmlWidget : public dt::Widget {
  private:
    std::ostringstream html;

  public:
    explicit HtmlWidget(DataTable* dt)
      : dt::Widget(dt, split_view_tag) {}

    py::oobj to_python() {
      render_all();
      const std::string htmlstr = html.str();
      return py::ostring(htmlstr);
    }


  protected:
    void _render() override {
      html << "<div class='datatable'>\n";
      html << "  <table class='frame'>\n";
      html << "  <thead>\n";
      _render_column_names();
      _render_column_types();
      html << "  </thead>\n";
      html << "  <tbody>\n";
      _render_data_rows();
      html << "  </tbody>\n";
      html << "  </table>\n";
      _render_table_footer();
      html << "</div>\n";
    }

    void _render_column_names() {
      const strvec& colnames = dt_->get_names();
      html << "    <tr class='colnames'>";
      if (render_row_indices_) {
        html << "<td class='row_index'></td>";
      }
      for (size_t j : colindices_) {
        if (j == dt::NA_index) {
          html << "<th class='vellipsis'>&hellip;</th>";
        } else {
          html << "<th>";
          _render_escaped_string(colnames[j].data(), colnames[j].size());
          html << "</th>";
        }
      }
      html << "</tr>\n";
    }

    void _render_column_types() {
      html << "    <tr class='coltypes'>";
      if (render_row_indices_) {
        html << "<td class='row_index'></td>";
      }
      for (size_t j : colindices_) {
        if (j == dt::NA_index) {
          html << "<td></td>";
        } else {
          auto stype_info = info(dt_->get_column(j).stype());
          size_t elemsize = stype_info.elemsize();
          html << "<td class='" << stype_info.ltype_name()
               << "' title='" << stype_info.name() << "'>";
          for (size_t k = 0; k < elemsize; ++k) html << "&#x25AA;";
          html << "</td>";
        }
      }
      html << "</tr>\n";
    }

    void _render_data_rows() {
      for (size_t i : rowindices_) {
        if (i == dt::NA_index) {
          _render_ellipsis_row();
        } else {
          _render_data_row(i);
        }
      }
    }

    void _render_ellipsis_row() {
      html << "    <tr>";
      if (render_row_indices_) {
        html << "<td class='row_index'>&#x22EE;</td>";
      }
      for (size_t j : colindices_) {
        if (j == dt::NA_index) {
          html << "<td class='hellipsis'>&#x22F1;</td>";
        } else {
          html << "<td class='hellipsis'>&#x22EE;</td>";
        }
      }
      html << "</tr>\n";
    }

    void _render_data_row(size_t i) {
      html << "    <tr>";
      if (render_row_indices_) {
        html << "<td class='row_index'>";
        _render_comma_separated(i);
        html << "</td>";
      }
      for (size_t j : colindices_) {
        if (j == dt::NA_index) {
          html << "<td class=vellipsis>&hellip;</td>";
          continue;
        }
        html << "<td>";
        const Column& col = dt_->get_column(j);
        switch (col.stype()) {
          case SType::BOOL:
          case SType::INT8:    _render_fw_value<int8_t>(col, i); break;
          case SType::INT16:   _render_fw_value<int16_t>(col, i); break;
          case SType::INT32:   _render_fw_value<int32_t>(col, i); break;
          case SType::INT64:   _render_fw_value<int64_t>(col, i); break;
          case SType::FLOAT32: _render_fw_value<float>(col, i); break;
          case SType::FLOAT64: _render_fw_value<double>(col, i); break;
          case SType::STR32:
          case SType::STR64:   _render_str_value(col, i); break;
          case SType::OBJ:     _render_obj_value(col, i); break;
          default:
            html << "(unknown stype)";
        }
        html << "</td>";
      }
      html << "</tr>\n";
    }


    void _render_table_footer() {
      size_t nrows = dt_->nrows();
      size_t ncols = dt_->ncols();
      html << "  <div class='footer'>\n";
      html << "    <div class='frame_dimensions'>";
      _render_comma_separated(nrows);
      html << " row" << (nrows == 1? "" : "s") << " &times; ";
      _render_comma_separated(ncols);
      html << " column" << (ncols == 1? "" : "s");
      html << "</div>\n";
      html << "  </div>\n";
    }


    void _render_escaped_string(const char* ch, size_t len) {
      size_t maxi = std::min(len, size_t(50));
      uint8_t uc;
      for (size_t i = 0; i < maxi; ++i) {
        char c = ch[i];
        if (c == '&') html << "&amp;";
        else if (c == '<') html << "&lt;";
        else if (c == '>') html << "&gt;";
        else {
          html << c;
          if (maxi < len && (uc = static_cast<uint8_t>(c)) >= 0xC0) {
            // for Unicode characters, make sure they are calculated as a
            // single char, and also not truncated in the middle.
            if ((uc & 0xE0) == 0xC0)      maxi += 1;
            else if ((uc & 0xF0) == 0xE0) maxi += 2;
            else if ((uc & 0xF8) == 0xF0) maxi += 3;
            if (maxi > len) maxi = len;
          }
        }
      }
      if (len > maxi) html << "&#133;";
    }

    template <typename T>
    void _render_fw_value(const Column& col, size_t row) {
      T val;
      bool isvalid = col.get_element(row, &val);
      if (isvalid) {
        if (val < 0) {
          html << "&minus;";
          val = -val;
        }
        html << +val; // "+" ensures that `int8_t` vals are rendered as numbers

      } else {
        _render_na();
      }
    }

    void _render_str_value(const Column& col, size_t row) {
      CString val;
      bool isvalid = col.get_element(row, &val);
      if (isvalid) {
        _render_escaped_string(val.ch, static_cast<size_t>(val.size));
      } else {
        _render_na();
      }
    }

    void _render_obj_value(const Column& col, size_t row) {
      py::robj val;
      bool isvalid = col.get_element(row, &val);
      if (isvalid) {
        // Should we use repr() here instead?
        py::ostring strval = val.to_pystring_force();
        CString cstr = strval.to_cstring();
        _render_escaped_string(cstr.ch, static_cast<size_t>(cstr.size));
      } else {
        _render_na();
      }
    }

    void _render_na() {
      html << "<span class=na>NA</span>";
    }

    void _render_comma_separated(size_t n) {
      // It is customary not to display commas in 4-digit numbers
      if (n < 10000) {
        html << n;
        return;
      }
      size_t n10 = n / 10;
      size_t k = 1;
      size_t m = 0;
      while (k <= n10) {
        k *= 10;
        ++m;
      }
      m = m % 3;
      while (k) {
        size_t d = n / k;
        n -= d * k;
        k /= 10;
        html << char('0' + d);
        if (m == 0 && k) {
          html << ',';
          m = 2;
        } else {
          --m;
        }
      }
    }
};




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
  HtmlWidget widget(dt);
  return widget.to_python();
}


static PKArgs args__repr_pretty_(
    0, 2, 0, false, false, {"p", "cycle"}, "_repr_pretty_", nullptr);

oobj Frame::_repr_pretty_(const PKArgs&) {
  if (dt::Terminal::standard_terminal().is_jupyter()) {
    return py::None();
  } else {
    return oobj(this).invoke("view", obool(false));
  }
}


static PKArgs args_view(
  0, 2, 0, false, false, {"interactive", "plain"}, "view", nullptr);

void Frame::view(const PKArgs& args) {
  bool interactive = true;  // default when `interactive` is omitted entirely
  bool plain = args[1].to<bool>(false);
  if (args[0].is_none()) interactive = dt::display_interactive;
  if (args[0].is_bool()) interactive = args[0].to_bool_strict();
  if (interactive && !dt::Terminal::standard_terminal().is_jupyter()) {
    interactive = false;
  }
  if (interactive) {
    oobj DFWidget = oobj::import("datatable")
                    .get_attr("widget")
                    .get_attr("DataFrameWidget");
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
  xt.add(METHOD(&Frame::_repr_pretty_, args__repr_pretty_));
  xt.add(METHOD(&Frame::view, args_view));
}


}  // namespace py
