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
#include "frame/repr/repr.h"
#include "utils/assert.h"
namespace dt {



TextColumn::TextColumn(const string& name, const Column& col,
                       const intvec& indices)
  : name_(name),
    width_(name.size())
{
  LType ltype = col.ltype();
  alignment_ = (ltype == LType::BOOL)? Align::RIGHT :
               (ltype == LType::INT) ? Align::RIGHT :
               (ltype == LType::REAL)? Align::DOT :
                                       Align::LEFT;
  margin_left_ = true;
  margin_right_ = true;
  _render_all_data(col, indices);
  _align_at_dot();
}



//------------------------------------------------------------------------------
// Print data to the output stream
//------------------------------------------------------------------------------

void TextColumn::print_name(ostringstream& out) const {
  _print_aligned_value(out, name_);
}


void TextColumn::print_separator(ostringstream& out) const {
  if (margin_left_) out << ' ';
  for (size_t i = 0; i < width_; ++i) {
    out << '-';
  }
  if (margin_right_) out << ' ';
}


void TextColumn::print_value(ostringstream& out, size_t i) const {
  _print_aligned_value(out, data_[i]);
}


void TextColumn::_print_aligned_value(ostringstream& out,
                                      const string& value) const
{
  if (margin_left_) out << ' ';
  if (alignment_ == Align::LEFT) {
    out << value;
    _print_whitespace(out, width_ - value.size());
  }
  if (alignment_ == Align::RIGHT) {
    _print_whitespace(out, width_ - value.size());
    out << value;
  }
  if (alignment_ == Align::DOT) {
    size_t i = 0;
    for (; i < value.size() && value[i] != '.'; ++i) {}
    _print_whitespace(out, width_left_ - i);
    out << value;
    _print_whitespace(out, width_ - value.size() - width_left_ + i);
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

static string _render_value_bool(const Column& col, size_t i) {
  int8_t value;
  bool isvalid = col.get_element(i, &value);
  if (!isvalid) return "";
  return value? "1" : "0";
}

template <typename T>
static string _render_value_int(const Column& col, size_t i) {
  T value;
  bool isvalid = col.get_element(i, &value);
  if (!isvalid) return "";
  return std::to_string(value);
}

template <typename T>
static string _render_value_float(const Column& col, size_t i) {
  T value;
  bool isvalid = col.get_element(i, &value);
  if (!isvalid) return "";
  ostringstream out;
  out << value;
  return out.str();
}


static std::string _render_value(const Column& col, size_t i) {
  switch (col.stype()) {
    case SType::BOOL:    return _render_value_bool(col, i);
    case SType::INT8:    return _render_value_int<int8_t>(col, i);
    case SType::INT16:   return _render_value_int<int16_t>(col, i);
    case SType::INT32:   return _render_value_int<int32_t>(col, i);
    case SType::INT64:   return _render_value_int<int64_t>(col, i);
    case SType::FLOAT32: return _render_value_float<float>(col, i);
    case SType::FLOAT64: return _render_value_float<double>(col, i);
    default: return "(?)";
  }
}




void TextColumn::_render_all_data(const Column& col, const intvec& indices) {
  data_.reserve(indices.size());
  for (size_t i : indices) {
    if (i == NA_index) {
      data_.push_back("...");
    } else {
      data_.push_back(_render_value(col, i));
    }
    size_t w = data_.back().size();
    if (width_ < w) width_ = w;
  }
}


void TextColumn::_align_at_dot() {
  if (alignment_ != Align::DOT) return;
  width_left_ = 0;
  for (const auto& val : data_) {
    size_t i = 0;
    for (; i < val.size() && val[i] != '.'; ++i) {}
    if (i > width_left_) width_left_ = i;
  }
}




}  // namespace dt
