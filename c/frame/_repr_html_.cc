//------------------------------------------------------------------------------
// Copyright 2018 H2O.ai
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
#include "frame/py_frame.h"
#include "python/string.h"
#include "types.h"

static const char* imgx =
    "url('data:image/png;base64,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"
    "QSFHy1p4fgAAAAASUVORK5CYII=');";
static const char* imgv =
    "url('data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAABIAAAAkCAYAAACE7WrnAAA"
    "AdElEQVR42mP4wyMVQQ3M8P///whqYBSDkG2A8bGJo+tBMQifIbgMQ5ZjwGUIPjY2wxiwOZWQZ"
    "rxhhM0F6IYjq8PqNWyBh4+NN7CpGv2jBo0aNGrQqEGjBtHFIIoLf5pUR2RXkFStsqnSiKBqs4b"
    "i6KdW0w8AxFl+XL1lK8wAAAAASUVORK5CYII=');";



class HtmlWidget {
  private:
    std::ostringstream html;
    DataTable* dt;
    size_t ncols, cols0, cols1;
    size_t nrows, rows0, rows1;
    static bool styles_emitted;

  public:
    explicit HtmlWidget(DataTable* dt_) {
      const size_t maxcols = 15;  // TODO: make configurable
      const size_t maxrows = 15;
      dt = dt_;
      ncols = dt->ncols;
      nrows = dt->nrows;
      cols0 = ncols <= maxcols ? ncols : maxcols * 2 / 3;
      cols1 = ncols <= maxcols ? 0 : maxcols - cols0;
      rows0 = nrows <= maxrows ? nrows : maxrows * 2 / 3;
      rows1 = nrows <= maxrows ? 0 : maxrows - rows0;
    }

    py::oobj to_pystring() {
      render_all();
      const std::string htmlstr = html.str();
      return py::ostring(htmlstr);
    }


  private:
    void render_all() {
      render_styles();
      html << "<div class='datatable'>\n";
      html << "  <table class='frame'>\n";
      render_table_header();
      render_table_body();
      html << "  </table>\n";
      render_table_footer();
      html << "</div>\n";
    }

    void render_table_header() {
      html << "  <thead>\n";
      render_column_names();
      render_column_types();
      html << "  </thead>\n";
    }

    void render_column_names() {
      const std::vector<std::string>& colnames = dt->get_names();
      html << "    <tr class='colnames'>";
      html << "<td class='row_index'></td>";
      for (size_t j = 0; j < ncols; ++j) {
        if (j == cols0) {
          j = ncols - cols1;
          html << "<th class='vellipsis'>&hellip;</th>";
        }
        html << "<th>";
        render_escaped_string(colnames[j].data(), colnames[j].size());
        html << "</th>";
      }
      html << "</tr>\n";
    }

    void render_column_types() {
      html << "    <tr class='coltypes'>";
      html << "<td class='row_index'></td>";
      for (size_t j = 0; j < ncols; ++j) {
        if (j == cols0) {
          j = ncols - cols1;
          html << "<th class='vellipsis'>&hellip;</th>";
        }
        SType stype = dt->columns[j]->stype();
        size_t elemsize = info(stype).elemsize();
        html << "<td class='" << info(stype).ltype_name()
             << "' title='" << info(stype).name() << "'>";
        for (size_t k = 0; k < elemsize; ++k) html << "&#x25AA;";
        html << "</td>";
      }
      html << "</tr>\n";
    }

    void render_table_body() {
      html << "  <tbody>\n";
      for (size_t i = 0; i < nrows; ++i) {
        if (i == rows0) {
          i = nrows - rows1;
          render_ellipsis_row();
        }
        render_data_row(i);
      }
      html << "  </tbody>\n";
    }

    void render_ellipsis_row() {
      html << "    <tr>";
      html << "<td class='row_index'>&#x22EE;</td>";
      for (size_t j = 0; j < ncols; ++j) {
        if (j == cols0) {
          j = ncols - cols1;
          html << "<td class='hellipsis'>&#x22F1;</td>";
        }
        html << "<td class='hellipsis'>&#x22EE;</td>";
      }
      html << "</tr>\n";
    }

    void render_data_row(size_t i) {
      html << "    <tr>";
      html << "<td class='row_index'>";
      render_comma_separated(i);
      html << "</td>";
      for (size_t j = 0; j < ncols; ++j) {
        if (j == cols0) {
          j = ncols - cols1;
          html << "<td class=vellipsis>&hellip;</td>";
        }
        html << "<td>";
        const Column* col = dt->columns[j];
        switch (col->stype()) {
          case SType::BOOL:
          case SType::INT8:    render_fw_value<int8_t>(col, i); break;
          case SType::INT16:   render_fw_value<int16_t>(col, i); break;
          case SType::INT32:   render_fw_value<int32_t>(col, i); break;
          case SType::INT64:   render_fw_value<int64_t>(col, i); break;
          case SType::FLOAT32: render_fw_value<float>(col, i); break;
          case SType::FLOAT64: render_fw_value<double>(col, i); break;
          case SType::STR32:   render_str_value<uint32_t>(col, i); break;
          case SType::STR64:   render_str_value<uint64_t>(col, i); break;
          case SType::OBJ:     render_obj_value(col, i); break;
          default:
            html << "(unknown stype)";
        }
        html << "</td>";
      }
      html << "</tr>\n";
    }


    void render_table_footer() {
      html << "  <div class='footer'>\n";
      render_frame_dimensions();
      html << "  </div>\n";
    }

    void render_frame_dimensions() {
      html << "    <div class='frame_dimensions'>";
      render_comma_separated(nrows);
      html << " row" << (nrows == 1? "" : "s") << " &times; ";
      render_comma_separated(ncols);
      html << " column" << (ncols == 1? "" : "s");
      html << "</div>\n";
    }


    void render_escaped_string(const char* ch, size_t len) {
      size_t maxi = std::min(len, size_t(50));
      for (size_t i = 0; i < maxi; ++i) {
        char c = ch[i];
        if (c == '&') html << "&amp;";
        else if (c == '<') html << "&lt;";
        else if (c == '>') html << "&gt;";
        else html << c;
      }
      if (len > maxi) html << "&#133;";
    }

    template <typename T>
    void render_fw_value(const Column* col, size_t row) {
      auto scol = static_cast<const FwColumn<T>*>(col);
      auto irow = scol->rowindex()[row];
      T val = scol->get_elem(irow);
      if (ISNA<T>(val)) render_na();
      else {
        if (val < 0) {
          html << "&minus;";
          val = -val;
        }
        if (std::is_integral<T>::value) html << static_cast<int64_t>(val);
        else html << val;
      }
    }

    template <typename T>
    void render_str_value(const Column* col, size_t row) {
      auto scol = static_cast<const StringColumn<T>*>(col);
      auto irow = scol->rowindex()[row];
      const T* offsets = scol->offsets();
      const char* strdata = scol->strdata();
      T str0 = offsets[irow - 1] & ~GETNA<T>();
      T str1 = offsets[irow];
      if (ISNA<T>(str1)) {
        render_na();
      } else {
        render_escaped_string(strdata + str0, str1 - str0);
      }
    }

    void render_obj_value(const Column* col, size_t row) {
      auto scol = static_cast<const PyObjectColumn*>(col);
      auto irow = scol->rowindex()[row];
      PyObject* val = scol->get_elem(irow);
      if (ISNA<PyObject*>(val)) render_na();
      else {
        // Should we use repr() here instead?
        py::ostring strval = py::robj(val).to_pystring_force();
        CString cstr = strval.to_cstring();
        render_escaped_string(cstr.ch, static_cast<size_t>(cstr.size));
      }
    }

    void render_na() {
      html << "<span class=na>NA</span>";
    }

    void render_styles() {
      if (styles_emitted) return;
      std::time_t now = std::time(nullptr);
      std::tm* t = std::localtime(&now);
      bool xd = (t->tm_mon == 11);
      bool vd = (t->tm_mon == 4) && (t->tm_wday == 4) &&
                (t->tm_mday >= 15) && (t->tm_mday <= 21);

      html << "<style type='text/css'>\n";
      html << ".datatable table.frame { margin-bottom: 0; }\n"
              ".datatable table.frame thead { border-bottom: none; }\n"
              ".datatable table.frame tr.coltypes td {"
              "  color: #FFFFFF;"
              "  line-height: 6px;"
              "  padding: 0 0.5em;"
              "}\n"
              ".datatable .bool { background: #DDDD99; }\n"
              ".datatable .obj  { background: #565656; }\n"
              ".datatable .int  { background: #5D9E5D; }\n"
              ".datatable .real { background: #4040CC; }\n"
              ".datatable .str  { background: #CC4040; }\n"
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
      styles_emitted = true;
    }

    void render_comma_separated(size_t n) {
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

bool HtmlWidget::styles_emitted = false;



//------------------------------------------------------------------------------
// py::Frame interface
//------------------------------------------------------------------------------
namespace py {

NoArgs Frame::Type::args__repr_html_("_repr_html_", nullptr);

oobj Frame::_repr_html_(const NoArgs&) {
  HtmlWidget widget(dt);
  return widget.to_pystring();
}

}
