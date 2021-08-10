//------------------------------------------------------------------------------
// Copyright 2019-2021 H2O.ai
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
#include <algorithm>                  // std::min, std::max
#include <iomanip>                    // std::setfill, std::setw
#include "csv/toa.h"
#include "frame/repr/repr_options.h"
#include "frame/repr/text_column.h"
#include "lib/hh/date.h"
#include "ltype.h"
#include "utils/assert.h"
#include "encodings.h"
#include "stype.h"
namespace dt {


//------------------------------------------------------------------------------
// TextColumn
//------------------------------------------------------------------------------

const Terminal* TextColumn::term_ = nullptr;
tstring TextColumn::ellipsis_;
tstring TextColumn::na_value_;
tstring TextColumn::true_value_;
tstring TextColumn::false_value_;


void TextColumn::setup(const Terminal* terminal) {
  term_ = terminal;
  na_value_ = tstring("NA", style::dim);
  ellipsis_ = tstring(term_->unicode_allowed()? "\xE2\x80\xA6" : "...",
                      style::dim|style::nocolor);
  true_value_ = tstring("1");
  false_value_ = tstring("0");
}


TextColumn::TextColumn() {
  width_ = 2;
  margin_left_ = true;
  margin_right_ = true;
}

TextColumn::~TextColumn() = default;



void TextColumn::unset_left_margin() {
  margin_left_ = false;
}

void TextColumn::unset_right_margin() {
  margin_right_ = false;
}

int TextColumn::get_width() const {
  return static_cast<int>(width_) + margin_left_ + margin_right_;
}




//------------------------------------------------------------------------------
// Data_TextColumn : public
//------------------------------------------------------------------------------

Data_TextColumn::Data_TextColumn(const std::string& name,
                                 const Column& col,
                                 const sztvec& indices,
                                 int max_width)
{
  xassert(max_width >= 4);
  // -2 takes into account column's margins
  max_width_ = std::min(max_width - 2, display_max_column_width);
  std::string type_name = col.type().to_string();
  name_ = _escape_string(CString(name));
  type_ = _escape_string(CString(type_name));
  width_ = std::max(std::max(width_, name_.size()),
                    name_.empty()? 0 : type_.size());
  align_right_ = col.type().is_numeric_or_void();
  margin_left_ = true;
  margin_right_ = true;
  _render_all_data(col, indices);
}


void Data_TextColumn::print_name(TerminalStream& out) const {
  _print_aligned_value(out, name_);
}


void Data_TextColumn::print_type(TerminalStream& out) const {
  if (name_.empty()) {
    out << std::string(margin_left_ + margin_right_ + width_, ' ');
  } else {
    _print_aligned_value(out, type_);
  }
}


void Data_TextColumn::print_separator(TerminalStream& out) const {
  out << std::string(margin_left_, ' ')
      << std::string(width_, '-')
      << std::string(margin_right_, ' ');
}


void Data_TextColumn::print_value(TerminalStream& out, size_t i) const {
  _print_aligned_value(out, data_[i]);
}



//------------------------------------------------------------------------------
// Data_TextColumn : private
//------------------------------------------------------------------------------

void Data_TextColumn::_print_aligned_value(TerminalStream& out,
                                           const tstring& value) const
{
  xassert(width_ >= value.size());
  auto indent = std::string(width_ - value.size(), ' ');
  out << std::string(margin_left_, ' ');
  if (align_right_) {
    out << indent << value;
  }
  else {
    out << value << indent;
  }
  out << std::string(margin_right_, ' ');
}


tstring Data_TextColumn::_render_value_bool(const Column& col, size_t i) const {
  int8_t value;
  bool isvalid = col.get_element(i, &value);
  if (!isvalid) return na_value_;
  return value? true_value_ : false_value_;
}

template <typename T>
tstring Data_TextColumn::_render_value_int(const Column& col, size_t i) const {
  T value;
  bool isvalid = col.get_element(i, &value);
  if (!isvalid) return na_value_;
  return tstring(std::to_string(value));
}

template <typename T>
tstring Data_TextColumn::_render_value_float(const Column& col, size_t i) const
{
  T value;
  bool isvalid = col.get_element(i, &value);
  if (!isvalid) return na_value_;
  std::ostringstream out;
  out << value;
  return tstring(out.str());
}

tstring Data_TextColumn::_render_value_date(const Column& col, size_t i) const {
  static char tmp[15];
  int32_t value;
  bool isvalid = col.get_element(i, &value);
  if (isvalid) {
    char* ch = tmp;
    date32_toa(&ch, value);
    return tstring(std::string(tmp, static_cast<size_t>(ch - tmp)));
  } else {
    return na_value_;
  }
}


tstring Data_TextColumn::_render_value_time(const Column& col, size_t i) const {
  static char tmp[30];
  int64_t value;
  bool isvalid = col.get_element(i, &value);
  if (isvalid) {
    char* ch = tmp;
    time64_toa(&ch, value);
    xassert(ch > tmp + 10);
    xassert(tmp[10] == 'T');
    tstring out;
    out << std::string(tmp, 10);
    out << tstring("T", style::dim);
    out << std::string(tmp + 11, static_cast<size_t>(ch - tmp - 11));
    return out;
  } else {
    return na_value_;
  }
}


tstring Data_TextColumn::_render_value_array(const Column& col, size_t i) const {
  Column value;
  bool isvalid = col.get_element(i, &value);
  if (isvalid) {
    bool allow_unicode = term_->unicode_allowed();
    int max_width0 = max_width_;
    // Leave 5 chars for [, …]
    int remaining_width = max_width_ - 5;
    tstring out;
    out << tstring("[");
    for (size_t j = 0; j < value.nrows(); j++) {
      if (j > 0) {
        out << tstring(", ");
        remaining_width -= 2;
      }
      const_cast<Data_TextColumn*>(this)->max_width_ = remaining_width;
      tstring repr = _render_value(value, j);
      out << repr;
      remaining_width -= repr.size();
      if (remaining_width <= 0) {
        out << tstring(", ");
        out << tstring(allow_unicode? "\xE2\x80\xA6" : "~", style::dim);
        break;
      }
    }
    out << tstring("]");
    const_cast<Data_TextColumn*>(this)->max_width_ = max_width0;
    return out;
  } else {
    return na_value_;
  }
}


bool Data_TextColumn::_needs_escaping(const CString& str) const {
  size_t n = str.size();
  if (static_cast<int>(n) > max_width_) return true;
  for (size_t i = 0; i < n; ++i) {
    auto c = static_cast<unsigned char>(str[i]);
    if (c < 0x20 || c >= 0x7E) return true;
  }
  return false;
}


static tstring _escaped_char(uint8_t a) {
  std::string escaped = (a == '\n')? "\\n" :
                        (a == '\t')? "\\t" :
                        (a == '\r')? "\\r" : "\\x00";
  if (escaped[1] == 'x') {
    int lo = a & 15;
    escaped[2] = static_cast<char>('0' + (a >> 4));
    escaped[3] = static_cast<char>((lo >= 10)? 'A' + lo - 10 : '0' + lo);
  }
  return tstring(escaped, style::dim);
}

static tstring _escape_unicode(int cp) {
  std::string escaped = (cp <= 0xFF)?   "\\x00" :
                        (cp <= 0xFFFF)? "\\u0000"
                                      : "\\U00000000";
  size_t i = escaped.size() - 1;
  while (cp) {
    int digit = cp & 15;
    escaped[i] = static_cast<char>((digit >= 10)? 'A' + digit - 10
                                                : '0' + digit);
    --i;
    cp >>= 4;
  }
  return tstring(escaped, style::dim);
}


/**
  * This function takes `str` as an input, and produces a formatted
  * output string, suitable for printing into the terminal. The
  * following transformations are applied:
  *
  *   - C0 & C1 control characters (including U+007F) are \-escaped;
  *   - any unicode characters are also escaped if the global option
  *     `allow_unicode` is false;
  *   - the output is limited to `max_width_` in size; if the input
  *     exceeds this limit, an ellipsis character would be added.
  */
tstring Data_TextColumn::_escape_string(const CString& str) const
{
  tstring out;

  // -1 because we leave 1 char space for the ellipsis character.
  // On the other hand, when we reach the end of `str` we'll
  // increase the `remaining_width` by 1 once again.
  int remaining_width = max_width_ - 1;
  bool allow_unicode = term_->unicode_allowed();

  size_t n = str.size();
  auto ch = reinterpret_cast<const unsigned char*>(str.data());
  auto end = ch + n;
  while (ch < end) {
    auto c = *ch;
    // printable ASCII
    if (c >= 0x20 && c <= 0x7E) {
      ch++;
      if (ch == end) remaining_width++;
      if (!remaining_width) break;
      out << c;
      remaining_width--;
    }
    // C0 block + \x7F (DEL) character
    else if (c <= 0x1F || c == 0x7F) {
      if (ch + 1 == end) remaining_width++;
      auto escaped = _escaped_char(c);
      if (static_cast<int>(escaped.size()) > remaining_width) break;
      remaining_width -= static_cast<int>(escaped.size());
      out << std::move(escaped);
      ch++;
    }
    // unicode character
    else {
      auto ch0 = ch;
      int cp = read_codepoint_from_utf8(&ch);  // advances `ch`
      if (ch == end) remaining_width++;
      if (allow_unicode && cp >= 0xA0) {  // excluding the C1 block
        int w = mk_wcwidth(cp);
        if (w > remaining_width) {
          ch = ch0;
          break;
        }
        for (; ch0 < ch; ++ch0) out << *ch0;
        remaining_width -= w;
      } else {
        auto escaped = _escape_unicode(cp);
        if (static_cast<int>(escaped.size()) > remaining_width) {
          ch = ch0;
          break;
        }
        remaining_width -= static_cast<int>(escaped.size());
        out << std::move(escaped);
      }
    }
  }
  // If we broke out of loop earlty, ellipsis needs to be added
  if (ch < end) {
    out << tstring(allow_unicode? "\xE2\x80\xA6" : "~",
                   style::dim);
  }
  return out;
}


tstring Data_TextColumn::_render_value_string(const Column& col, size_t i) const
{
  CString value;
  bool isvalid = col.get_element(i, &value);
  if (!isvalid) return na_value_;
  return _needs_escaping(value)? _escape_string(value)
                               : tstring(value.to_string());
}


tstring Data_TextColumn::_render_value(const Column& col, size_t i) const {
  switch (col.stype()) {
    case SType::VOID:    return na_value_;
    case SType::BOOL:    return _render_value_bool(col, i);
    case SType::INT8:    return _render_value_int<int8_t>(col, i);
    case SType::INT16:   return _render_value_int<int16_t>(col, i);
    case SType::INT32:   return _render_value_int<int32_t>(col, i);
    case SType::INT64:   return _render_value_int<int64_t>(col, i);
    case SType::FLOAT32: return _render_value_float<float>(col, i);
    case SType::FLOAT64: return _render_value_float<double>(col, i);
    case SType::STR32:
    case SType::STR64:   return _render_value_string(col, i);
    case SType::DATE32:  return _render_value_date(col, i);
    case SType::TIME64:  return _render_value_time(col, i);
    case SType::ARR32:
    case SType::ARR64:   return _render_value_array(col, i);
    default: return tstring("<unknown>", style::dim);
  }
}



void Data_TextColumn::_render_all_data(const Column& col, const sztvec& indices)
{
  data_.reserve(indices.size());
  for (size_t i : indices) {
    if (i == NA_index) {
      data_.push_back(ellipsis_);
    } else {
      auto rendered_value = _render_value(col, i);
      data_.push_back(std::move(rendered_value));
    }
    size_t w = data_.back().size();
    if (width_ < w) width_ = w;
  }
  if (col.ltype() == LType::REAL) {
    _align_at_dot();
  }
}


void Data_TextColumn::_align_at_dot() {
  size_t n = data_.size();
  std::vector<size_t> right_widths;
  right_widths.reserve(n);

  size_t max_right_width = 0;
  for (size_t i = 0; i < n; ++i) {
    const auto& str = data_[i].str();
    size_t k = str.size();
    if (k == data_[i].size()) {
      for (; k > 0 && str[k-1] != '.'; --k) {}
      if (k) k = str.size() - k;
      if (k > max_right_width) max_right_width = k;
    } else {
      k = NA_index;
    }
    right_widths.push_back(k);
  }

  for (size_t i = 0; i < n; ++i) {
    size_t w = right_widths[i];
    if (w == NA_index) continue;
    if (w < max_right_width) {
      size_t nspaces = max_right_width - w + (w == 0);
      data_[i] = tstring(std::string(data_[i].str())
                         .insert(data_[i].str().size(), nspaces, ' '));
      width_ = std::max(width_, data_[i].size());
    }
  }
}



//------------------------------------------------------------------------------
// VSep_TextColumn
//------------------------------------------------------------------------------

VSep_TextColumn::VSep_TextColumn() : TextColumn() {
  width_ = 1;
  margin_left_ = false;
  margin_right_ = false;
}


void VSep_TextColumn::print_name(TerminalStream& out) const {
  out << tstring("|", style::nobold|style::grey);
}

void VSep_TextColumn::print_type(TerminalStream& out) const {
  out << tstring("|", style::nobold|style::nodim|style::noitalic|style::grey);
}

void VSep_TextColumn::print_separator(TerminalStream& out) const {
  out << '+';
}

void VSep_TextColumn::print_value(TerminalStream& out, size_t) const {
  out << tstring("|", style::grey);
}



//------------------------------------------------------------------------------
// Ellipsis_TextColumn
//------------------------------------------------------------------------------

Ellipsis_TextColumn::Ellipsis_TextColumn() : TextColumn() {
  ell_ = tstring(term_->unicode_allowed()? " \xE2\x80\xA6 " : " ~ ",
                 style::dim|style::nobold);
  space_ = tstring("   ");
  width_ = 1;
  margin_left_ = true;
  margin_right_ = true;
}


void Ellipsis_TextColumn::print_name(TerminalStream& out) const {
  out << ell_;
}

void Ellipsis_TextColumn::print_type(TerminalStream& out) const {
  out << space_;
}

void Ellipsis_TextColumn::print_separator(TerminalStream& out) const {
  out << space_;
}

void Ellipsis_TextColumn::print_value(TerminalStream& out, size_t) const {
  out << ell_;
}



//------------------------------------------------------------------------------
// RowIndex_TextColumn
//------------------------------------------------------------------------------

RowIndex_TextColumn::RowIndex_TextColumn(const sztvec& indices) {
  row_numbers_ = indices;
  width_ = 0;
  if (!indices.empty()) {
    size_t max_value = indices.back();
    if (max_value == NA_index) {
      max_value = indices.size() >= 2? indices[indices.size() - 2] : 0;
    }
    while (max_value) {
      width_++;
      max_value /= 10;
    }
  }
  if (width_ < 2) width_ = 2;
  if (width_ < ellipsis_.size()) {
    bool has_ellipsis = false;
    for (size_t k : row_numbers_) {
      if (k == NA_index) {
        has_ellipsis = true;
        break;
      }
    }
    if (has_ellipsis) {
      width_ = ellipsis_.size();
    }
  }
  margin_left_ = false;
  margin_right_ = true;
}


void RowIndex_TextColumn::print_name(TerminalStream& out) const {
  out << std::string(width_ + 1, ' ');
}

void RowIndex_TextColumn::print_type(TerminalStream& out) const {
  out << std::string(width_ + 1, ' ');
}

void RowIndex_TextColumn::print_separator(TerminalStream& out) const {
  out << std::string(width_, '-')
      << " ";
}

void RowIndex_TextColumn::print_value(TerminalStream& out, size_t i) const {
  size_t row_index = row_numbers_[i];
  if (row_index == NA_index) {
    out << std::string(width_ - ellipsis_.size(), ' ')
        << ellipsis_ << " ";
  }
  else {
    auto rendered_value = std::to_string(row_numbers_[i]);
    xassert(width_ >= rendered_value.size());
    out << style::grey
        << std::string(width_ - rendered_value.size(), ' ')
        << rendered_value
        << " "
        << style::end;
  }
}




}  // namespace dt
