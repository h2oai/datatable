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
#include "frame/repr/text_column.h"
#include "utils/assert.h"
namespace dt {


//------------------------------------------------------------------------------
// Construction
//------------------------------------------------------------------------------
const Terminal& TextColumn::term = Terminal::get_instance();


TextColumn::TextColumn(const std::string& name, const Column& col,
                       const intvec& indices, bool is_key_column)
  : name_(name)
{
  width_ = std::max(name_.size(), size_t(2));
  LType ltype = col.ltype();
  align_right_ = (ltype == LType::BOOL) ||
                 (ltype == LType::INT) ||
                 (ltype == LType::REAL);
  margin_left_ = true;
  margin_right_ = true;
  is_key_column_ = is_key_column;
  _render_all_data(col, indices);
  if (ltype == LType::REAL) {
    _align_at_dot();
  }
}




//------------------------------------------------------------------------------
// Print data to the output stream
//------------------------------------------------------------------------------

void TextColumn::print_name(ostringstream& out) const {
  _print_aligned_value(out, term.bold(name_.str()));
}


void TextColumn::print_separator(ostringstream& out) const {
  if (margin_left_) out << ' ';
  out << term.grey(std::string(width_, '-'));
  if (margin_right_) out << ' ';
}


void TextColumn::print_value(ostringstream& out, size_t i) const {
  _print_aligned_value(out, data_[i].str());
}


void TextColumn::_print_aligned_value(ostringstream& out,
                                      const sstring& value) const
{
  if (margin_left_) out << ' ';
  if (align_right_) {
    _print_whitespace(out, width_ - value.size());
    out << value.str();
  }
  else {
    out << value.str();
    _print_whitespace(out, width_ - value.size());
  }
  if (margin_right_) out << ' ';
}


void TextColumn::_print_whitespace(ostringstream& out, size_t n) const {
  xassert(n < 10000);
  for (size_t i = 0; i < n; ++i) out << ' ';
}



//------------------------------------------------------------------------------
// Data rendering
//------------------------------------------------------------------------------

sstring TextColumn::_render_value_bool(const Column& col, size_t i) const {
  int8_t value;
  bool isvalid = col.get_element(i, &value);
  if (!isvalid) return sstring();
  return value? sstring("1", 1)
              : sstring("0", 1);
}

template <typename T>
sstring TextColumn::_render_value_int(const Column& col, size_t i) const {
  T value;
  bool isvalid = col.get_element(i, &value);
  if (!isvalid) return sstring();
  return sstring(std::to_string(value));
}

template <typename T>
sstring TextColumn::_render_value_float(const Column& col, size_t i) const {
  T value;
  bool isvalid = col.get_element(i, &value);
  if (!isvalid) return sstring();
  ostringstream out;
  out << value;
  return sstring(out.str());
}

sstring TextColumn::_render_value_string(const Column& col, size_t i) const {
  CString value;
  bool isvalid = col.get_element(i, &value);
  if (!isvalid) return sstring(term.dim("NA"), 2);
  return sstring(std::string(value.ch, static_cast<size_t>(value.size)));
}


sstring TextColumn::_render_value(const Column& col, size_t i) const {
  switch (col.stype()) {
    case SType::BOOL:    return _render_value_bool(col, i);
    case SType::INT8:    return _render_value_int<int8_t>(col, i);
    case SType::INT16:   return _render_value_int<int16_t>(col, i);
    case SType::INT32:   return _render_value_int<int32_t>(col, i);
    case SType::INT64:   return _render_value_int<int64_t>(col, i);
    case SType::FLOAT32: return _render_value_float<float>(col, i);
    case SType::FLOAT64: return _render_value_float<double>(col, i);
    case SType::STR32:
    case SType::STR64:   return _render_value_string(col, i);
    default: return sstring("", 0);
  }
}




void TextColumn::_render_all_data(const Column& col, const intvec& indices) {
  data_.reserve(indices.size());
  for (size_t i : indices) {
    if (i == NA_index) {
      data_.push_back(sstring("...", 3));
    } else {
      auto rendered_value = _render_value(col, i);
      if (is_key_column_) {
        rendered_value = sstring(term.grey(rendered_value.str()));
      }
      data_.push_back(std::move(rendered_value));
    }
    size_t w = data_.back().size();
    if (width_ < w) width_ = w;
  }
}


void TextColumn::_align_at_dot() {
  size_t n = data_.size();
  std::vector<size_t> right_widths;
  right_widths.reserve(n);

  size_t max_right_width = 0;
  for (size_t i = 0; i < n; ++i) {
    const auto& str = data_[i].str();
    size_t k = str.size();
    for (; k > 0 && str[k-1] != '.'; --k) {}
    k = str.size() - k;
    if (k > max_right_width) max_right_width = k;
    right_widths.push_back(k);
  }

  for (size_t i = 0; i < n; ++i) {
    if (right_widths[i] < max_right_width) {
      size_t nspaces = max_right_width - right_widths[i];
      data_[i] = sstring(std::string(data_[i].str())
                         .insert(data_[i].str().size(), nspaces, ' '));
      width_ = std::max(width_, data_[i].size());
    }
  }
}




}  // namespace dt
