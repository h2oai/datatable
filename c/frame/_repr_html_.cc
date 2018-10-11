//------------------------------------------------------------------------------
// Copyright 2018 H2O.ai
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//------------------------------------------------------------------------------
#include "frame/py_frame.h"
#include <sstream>
#include "python/string.h"
#include "types.h"



class HtmlWidget {
  private:
    std::ostringstream html;
    DataTable* dt;
    size_t ncols, cols0, cols1;
    size_t nrows, rows0, rows1;
    static bool styles_emitted;

  public:
    HtmlWidget(DataTable* dt_) {
      const size_t maxcols = 30;  // TODO: make configurable
      const size_t maxrows = 45;
      dt = dt_;
      ncols = static_cast<size_t>(dt->ncols);
      nrows = static_cast<size_t>(dt->nrows);
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
      html << "<div class=datatable>";
      html << "<table>";
      render_table_header();
      render_table_body();
      html << "</table>";
      render_frame_dimensions();
      html << "</div>";
    }

    void render_table_header() {
      const std::vector<std::string>& colnames = dt->get_names();
      html << "<thead><tr>";
      html <<   "<td class=row_index></td>";
      for (size_t j = 0; j < ncols; ++j) {
        if (j == cols0) {
          j = ncols - cols1;
          html << "<th clas=vellipsis>&hellip;</th>";
        }
        html << "<th>";
        render_escaped_string(colnames[j].data(), colnames[j].size());
        html << "</th>";
      }
      html << "</tr></thead>";
    }

    void render_table_body() {
      html << "<tbody>";
      for (size_t i = 0; i < nrows; ++i) {
        if (i == rows0) {
          i = nrows - rows1;
          render_ellipsis_row();
        }
        render_data_row(i);
      }
      html << "</tbody>";
    }

    void render_ellipsis_row() {
      html << "<tr>";
      html << "<td class=hellipsis>&middot;&middot;&middot;</td>";
      for (size_t j = 0; j < ncols; ++j) {
        if (j == cols0) {
          j = ncols - cols1;
          html << "<td></td>";
        }
        html << "<td class=hellipsis>&middot;&middot;&middot;</td>";
      }
      html << "</tr>";
    }

    void render_data_row(size_t i) {
      html << "<tr>";
      html << "<td class=row_index>" << i << "</td>";
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
          default:
            html << "(unknown stype)";
        }
        html << "</td>";
      }
      html << "</tr>";
    }

    void render_frame_dimensions() {
      html << "<div class=frame_dimensions>";
      render_comma_separated(nrows);
      html << " row" << (nrows == 1? "" : "s") << " &times; ";
      render_comma_separated(ncols);
      html << " column" << (ncols == 1? "" : "s");
      html << "</div>";
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
      T val = scol->get_elem(static_cast<int64_t>(row));
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
      const T* offsets = scol->offsets();
      const char* strdata = scol->strdata();
      T str0 = offsets[row - 1] & ~GETNA<T>();
      T str1 = offsets[row];
      if (ISNA<T>(str1)) {
        render_na();
      } else {
        render_escaped_string(strdata + str0, str1 - str0);
      }
    }

    void render_na() {
      html << "<span class=na>NA</span>";
    }

    void render_styles() {
      if (styles_emitted) return;
      html << "<style type='text/css'>"
              ".datatable .row_index {"
              "  color: #CDE;"
              "  background: rgba(255,255,255,0.75);"
              "  font-size: 80%;"
              "  border-right: 1px solid #CCE6FF;"
              "}"
              ".datatable .hellipsis { "
              "  background: linear-gradient(to bottom, "
              "    rgba(0,0,0,0.5) 0%, rgba(0,0,0,0.15) 7%, "
              "    rgba(0,0,0,0.0) 15%, rgba(0,0,0,0.0) 95%, "
              "    rgba(0,0,0,0.3) 100%);"
              "  padding: 0.1em 0.5em;"
              "  color: #DDD;"
              "}"
              ".datatable .vellipsis {"
              "  background: #FFF;"
              "  color: #DDD;"
              "  padding: 0.5em 1em;"
              "  border: 1px solid #EEE;"
              "  border-style: none solid;"
              "}"
              ".datatable th.vellipsis { border: none; }"
              ".datatable .na { color: #DDD; font-size: 80%; }"
              ".datatable .frame_dimensions {"
              "  background: #FAFAFA;"
              "  display: inline-block;"
              "  font-size: 80%;"
              "  color: #AAA;"
              "  border: 1px solid #EEE;"
              "  padding: 0.1em .5em;"
              "  margin-left: 2em;"
              "}"
              "</style>";
      styles_emitted = true;
    }

    void render_comma_separated(size_t n) {
      if (n == 0) {
        html << '0';
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
