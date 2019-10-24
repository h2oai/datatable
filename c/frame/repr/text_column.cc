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
#include "frame/repr/repr_options.h"
#include "frame/repr/text_column.h"
#include "utils/assert.h"
#include "encodings.h"
namespace dt {


//------------------------------------------------------------------------------
// TextColumn
//------------------------------------------------------------------------------

const Terminal* TextColumn::term_ = nullptr;
sstring TextColumn::ellipsis_;
sstring TextColumn::na_value_;

void TextColumn::setup(const Terminal* terminal) {
  term_ = terminal;
  na_value_ = sstring(term_->dim("NA"));
  ellipsis_ = term_->unicode_allowed()? sstring(term_->dim("\xE2\x80\xA6"))
                                      : sstring(term_->dim("..."));
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
                                 const intvec& indices,
                                 int max_width)
{
  max_width_ = std::min(max_width, display_max_column_width);
  name_ = sstring(_escape_string(name));
  width_ = std::max(width_, name_.size());
  LType ltype = col.ltype();
  align_right_ = (ltype == LType::BOOL) ||
                 (ltype == LType::INT) ||
                 (ltype == LType::REAL);
  margin_left_ = true;
  margin_right_ = true;
  _render_all_data(col, indices);
  if (ltype == LType::REAL) {
    _align_at_dot();
  }
}


void Data_TextColumn::print_name(ostringstream& out) const {
  _print_aligned_value(out, name_);
}


void Data_TextColumn::print_separator(ostringstream& out) const {
  if (margin_left_) out << ' ';
  out << std::string(width_, '-');
  if (margin_right_) out << ' ';
}


void Data_TextColumn::print_value(ostringstream& out, size_t i) const {
  _print_aligned_value(out, data_[i]);
}



//------------------------------------------------------------------------------
// Data_TextColumn : private
//------------------------------------------------------------------------------

void Data_TextColumn::_print_aligned_value(ostringstream& out,
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


void Data_TextColumn::_print_whitespace(ostringstream& out, size_t n) const {
  for (size_t i = 0; i < n; ++i) out << ' ';
}


sstring Data_TextColumn::_render_value_bool(const Column& col, size_t i) const {
  int8_t value;
  bool isvalid = col.get_element(i, &value);
  if (!isvalid) return na_value_;
  return value? sstring("1", 1)
              : sstring("0", 1);
}

template <typename T>
sstring Data_TextColumn::_render_value_int(const Column& col, size_t i) const {
  T value;
  bool isvalid = col.get_element(i, &value);
  if (!isvalid) return na_value_;
  return sstring(std::to_string(value));
}

template <typename T>
sstring Data_TextColumn::_render_value_float(const Column& col, size_t i) const
{
  T value;
  bool isvalid = col.get_element(i, &value);
  if (!isvalid) return na_value_;
  ostringstream out;
  out << value;
  return sstring(out.str());
}


bool Data_TextColumn::_needs_escaping(const CString& str) const {
  if (str.size > max_width_) return true;
  size_t n = static_cast<size_t>(str.size);
  for (size_t i = 0; i < n; ++i) {
    auto c = static_cast<unsigned char>(str.ch[i]);
    if (c < 0x20 || c >= 0x7E) return true;
  }
  return false;
}


static std::string _escaped_char(uint8_t a) {
  std::string escaped = (a == '\n')? "\\n" :
                        (a == '\t')? "\\t" :
                        (a == '\r')? "\\r" : "\\x00";
  if (escaped[1] == 'x') {
    int lo = a & 15;
    escaped[2] = static_cast<char>('0' + (a >> 4));
    escaped[3] = static_cast<char>((lo >= 10)? 'A' + lo - 10 : '0' + lo);
  }
  return escaped;
}

static std::string _escape_unicode(int cp) {
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
  return escaped;
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
std::string Data_TextColumn::_escape_string(const CString& str) const
{
  ostringstream out;
  // -1 because we leave 1 char space for the ellipsis character.
  // On the other hand, when we reach the end of `str` we'll
  // increase the `remaining_width` by 1 once again.
  int remaining_width = max_width_ - 1;
  bool allow_unicode = term_->unicode_allowed();

  size_t n = static_cast<size_t>(str.size);
  auto ch = reinterpret_cast<const unsigned char*>(str.ch);
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
      ch++;
      if (ch == end) remaining_width++;
      auto escaped = _escaped_char(c);
      if (static_cast<int>(escaped.size()) > remaining_width) break;
      out << term_->dim() << escaped << term_->reset();
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
        out << term_->dim() << escaped << term_->reset();
        remaining_width -= escaped.size();
      }
    }
  }
  // If we broke out of loop earlty, ellipsis needs to be added
  if (ch < end) {
    out << term_->dim();
    out << (allow_unicode? "\xE2\x80\xA6" : "~");
    out << term_->reset();
  }
  return out.str();
}


sstring Data_TextColumn::_render_value_string(const Column& col, size_t i) const
{
  CString value;
  bool isvalid = col.get_element(i, &value);
  if (!isvalid) return na_value_;
  return _needs_escaping(value)? sstring(_escape_string(value))
                               : sstring(value.to_string());
}


sstring Data_TextColumn::_render_value(const Column& col, size_t i) const {
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



void Data_TextColumn::_render_all_data(const Column& col, const intvec& indices)
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
      data_[i] = sstring(std::string(data_[i].str())
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


void VSep_TextColumn::print_name(ostringstream& out) const {
  out << term_->reset() << term_->grey("|") << term_->bold();
}

void VSep_TextColumn::print_separator(ostringstream& out) const {
  out << '+';
}

void VSep_TextColumn::print_value(ostringstream& out, size_t) const {
  out << term_->grey("|");
}



//------------------------------------------------------------------------------
// Ellipsis_TextColumn
//------------------------------------------------------------------------------

Ellipsis_TextColumn::Ellipsis_TextColumn() : TextColumn() {
  ell_ = term_->unicode_allowed()? sstring("\xE2\x80\xA6")  // ...
                                 : sstring("~");
  width_ = 1;
  margin_left_ = true;
  margin_right_ = true;
}


void Ellipsis_TextColumn::print_name(ostringstream& out) const {
  if (margin_left_) out << ' ';
  out << ell_.str();
  if (margin_right_) out << ' ';
}

void Ellipsis_TextColumn::print_separator(ostringstream& out) const {
  if (margin_left_) out << ' ';
  out << ell_.str();
  if (margin_right_) out << ' ';
}

void Ellipsis_TextColumn::print_value(ostringstream& out, size_t) const {
  if (margin_left_) out << ' ';
  out << term_->dim(ell_.str());
  if (margin_right_) out << ' ';
}



}  // namespace dt
