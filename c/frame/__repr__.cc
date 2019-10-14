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
#include "options.h"
#include "types.h"


//------------------------------------------------------------------------------
// Display options
//------------------------------------------------------------------------------

static constexpr size_t NA_size_t = size_t(-1);
static size_t display_max_nrows = 50;
static size_t display_head_nrows = 20;
static size_t display_tail_nrows = 10;

void py::Frame::init_display_options()
{
  dt::register_option(
    "display.max_nrows",
    []{
      return (display_max_nrows == NA_size_t)? py::None()
                                             : py::oint(display_max_nrows);
    },
    [](const py::Arg& value) {
      if (value.is_none()) {
        display_max_nrows = NA_size_t;
      } else {
        int64_t n = value.to_int64_strict();
        display_max_nrows = (n < 0)? NA_size_t : static_cast<size_t>(n);
      }
    },
    "A frame with more rows than this will be displayed truncated\n"
    "when the frame is printed to the console: only its first `head_nrows`\n"
    "and last `tail_nrows` rows will be printed. It is recommended to have\n"
    "`head_nrows + tail_nrows <= max_nrows`.\n"
    "Setting this option to None (or a negative value) will cause all\n"
    "rows in a frame to be printed, which may cause the console to become\n"
    "unresponsive.\n"
  );

  dt::register_option(
    "display.head_nrows",
    []{
      return py::oint(display_head_nrows);
    },
    [](const py::Arg& value) {
      display_head_nrows = value.to_size_t();
    },
    "The number of rows from the top of a frame to be displayed when\n"
    "the frame's output is truncated due to the total number of frame's\n"
    "rows exceeding `max_nrows` value.\n"
  );

  dt::register_option(
    "display.tail_nrows",
    []{
      return py::oint(display_tail_nrows);
    },
    [](const py::Arg& value) {
      display_tail_nrows = value.to_size_t();
    },
    "The number of rows from the bottom of a frame to be displayed when\n"
    "the frame's output is truncated due to the total number of frame's\n"
    "rows exceeding `max_nrows` value.\n"
  );
}



static const char* imgx =
    "url('data:image/png;base64,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"
    "QSFHy1p4fgAAAAASUVORK5CYII=');";
static const char* imgv =
    "url('data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAABIAAAAkCAYAAACE7WrnAAA"
    "AdElEQVR42mP4wyMVQQ3M8P///whqYBSDkG2A8bGJo+tBMQifIbgMQ5ZjwGUIPjY2wxiwOZWQZ"
    "rxhhM0F6IYjq8PqNWyBh4+NN7CpGv2jBo0aNGrQqEGjBtHFIIoLf5pUR2RXkFStsqnSiKBqs4b"
    "i6KdW0w8AxFl+XL1lK8wAAAAASUVORK5CYII=');";

static bool in_jupyter() {
  static bool jup = py::oobj::import("datatable")
                      .get_attr("utils")
                      .get_attr("terminal")
                      .get_attr("term")
                      .get_attr("jupyter")
                      .to_bool_strict();
  return jup;
}



//------------------------------------------------------------------------------
// Abstract class Widget
//------------------------------------------------------------------------------
static constexpr size_t NA_index = size_t(-1);


class Widget {
  private:
    size_t ncols_, nrows_, nkeys_;
    size_t startcol_, startrow_;
    size_t cols0_, cols1_;
    size_t rows0_, rows1_;

  protected:
    DataTable* dt_;
    std::vector<size_t> colindices_;
    std::vector<size_t> rowindices_;
    bool render_row_indices_;
    size_t : 56;

  private:
    explicit Widget(DataTable* dt) {
      dt_ = dt;
      ncols_ = dt->ncols();
      nrows_ = dt->nrows();
      nkeys_ = dt->nkeys();
      render_row_indices_ = true;
    }

  public:
    static struct SplitViewTag {} split_view_tag;
    static struct WindowedTag {} windowed_tag;

    Widget(DataTable* dt, SplitViewTag) : Widget(dt)
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

    Widget(DataTable* dt, WindowedTag) : Widget(dt)
    {
      startcol_ = 0;
      startrow_ = 0;
      cols0_ = 15;
      rows0_ = 30;
    }

    virtual ~Widget() = default;

    void render_all() {
      _generate_column_indices();
      _generate_row_indices();
      _render();
    }

  protected:
    virtual void _render() = 0;


  private:
    // Populate array `colindices_` with indices of the columns that shall be
    // rendered. The array may also contain `NA_index`, which indicates an
    // "ellipsis" column.
    void _generate_column_indices() {
      colindices_.clear();
      if (startcol_ == NA_index) {
        colindices_.reserve(cols0_ + cols1_ + 1);
        for (size_t i = 0; i < ncols_; ++i) {
          if (i == cols0_) {
            colindices_.push_back(NA_index);
            i = ncols_ - cols1_;
          }
          colindices_.push_back(i);
        }
      } else {
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
    void _generate_row_indices() {
      rowindices_.clear();
      if (startrow_ == NA_index) {
        rowindices_.reserve(rows0_ + rows1_ + 1);
        for (size_t i = 0; i < nrows_; ++i) {
          if (i == rows0_) {
            rowindices_.push_back(NA_index);
            i = nrows_ - rows1_;
          }
          rowindices_.push_back(i);
        }
      } else {
        rowindices_.reserve(rows0_);
        for (size_t i = 0; i < rows0_; ++i) {
          rowindices_.push_back(i + startrow_);
        }
      }
    }
};




//------------------------------------------------------------------------------
// HtmlWidget
//------------------------------------------------------------------------------

/**
  * This class is responsible for rendering a Frame into HTML.
  */
class HtmlWidget : public Widget {
  private:
    std::ostringstream html;
    static bool styles_emitted;

  public:
    explicit HtmlWidget(DataTable* dt)
      : Widget(dt, split_view_tag) {}

    py::oobj to_python() {
      render_all();
      const std::string htmlstr = html.str();
      return py::ostring(htmlstr);
    }


  protected:
    void _render() override {
      _render_styles();
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

    void _render_styles() {
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

bool HtmlWidget::styles_emitted = false;



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
  oobj DFWidget = oobj::import("datatable")
                  .get_attr("widget")
                  .get_attr("DataFrameWidget");
  return DFWidget.call({oobj(this)}).invoke("as_string");
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
  if (in_jupyter()) {
    return py::None();
  } else {
    return oobj(this).invoke("view", otuple{obool(false)});
  }
}


static PKArgs args_view(
  0, 1, 0, false, false, {"interactive"}, "view", nullptr);

void Frame::view(const PKArgs& args) {
  oobj interactive = args[0].is_undefined()? obool(true) : args[0].to_oobj();
  oobj DFWidget = oobj::import("datatable")
                  .get_attr("widget")
                  .get_attr("DataFrameWidget");
  DFWidget.call({oobj(this), interactive}).invoke("render");
}



void Frame::_init_repr(XTypeMaker& xt) {
  xt.add(METHOD__REPR__(&Frame::m__repr__));
  xt.add(METHOD__STR__(&Frame::m__str__));
  xt.add(METHOD(&Frame::_repr_html_, args__repr_html_));
  xt.add(METHOD(&Frame::_repr_pretty_, args__repr_pretty_));
  xt.add(METHOD(&Frame::view, args_view));
}


}  // namespace py
