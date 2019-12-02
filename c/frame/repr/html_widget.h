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
#ifndef dt_FRAME_REPR_HTML_WIDGET_h
#define dt_FRAME_REPR_HTML_WIDGET_h
#include "frame/repr/widget.h"
#include "python/_all.h"
#include "python/string.h"
#include "utils/terminal/terminal.h"
namespace dt {


void emit_stylesheet();

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

    static void write_to_jupyter(const py::oobj& htmlstr) {
    	auto update_kwds = py::odict();
    	write_to_jupyter(htmlstr, update_kwds);
    }

    static void write_to_jupyter(const py::oobj& htmlstr,
                                 const py::odict& update_kwds)
    {
      if (!dt::Terminal::standard_terminal().is_jupyter()) {
        return;
      }

      auto HTML = py::oobj::import("IPython.core.display", "HTML");
      auto display = py::oobj::import("IPython.core.display", "display");
      if (!update_kwds.empty()) {
        auto update = py::oobj::import("IPython.core.display", "update_display");
        update.call(HTML.call(), update_kwds);
      }

      display.call(HTML.call(htmlstr), update_kwds);
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
        if (j == NA_index) {
          html << "<th class='vellipsis'>&hellip;</th>";
        } else {
          html << (j < nkeys_? "<th class='row_index'>" : "<th>");
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
        if (j == NA_index) {
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
        if (i == NA_index) {
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
        if (j == NA_index) {
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
        if (j == NA_index) {
          html << "<td class=vellipsis>&hellip;</td>";
          continue;
        }
        html << (j < nkeys_? "<td class='row_index'>" : "<td>");
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


}  // namespace dt
#endif
