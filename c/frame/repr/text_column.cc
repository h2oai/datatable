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
  na_value_ = term_->dim("NA");
  ellipsis_ = term_->unicode_allowed()? term_->dim("\xE2\x80\xA6")  // '…'
                                      : term_->dim("...");
}


TextColumn::TextColumn() {
  width_ = 2;
  margin_left_ = true;
  margin_right_ = true;
  is_key_column_ = false;
}

TextColumn::~TextColumn() = default;



void TextColumn::unset_left_margin() {
  margin_left_ = false;
}

void TextColumn::unset_right_margin() {
  margin_right_ = false;
}




//------------------------------------------------------------------------------
// Data_TextColumn : public
//------------------------------------------------------------------------------

Data_TextColumn::Data_TextColumn(const std::string& name,
                                 const Column& col,
                                 const intvec& indices,
                                 bool is_key_column)
  : TextColumn(), name_(name)
{
  width_ = std::max(width_, name_.size());
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


void Data_TextColumn::print_name(ostringstream& out) const {
  _print_aligned_value(out, name_.str());
}


void Data_TextColumn::print_separator(ostringstream& out) const {
  if (margin_left_) out << ' ';
  out << std::string(width_, '-');
  if (margin_right_) out << ' ';
}


void Data_TextColumn::print_value(ostringstream& out, size_t i) const {
  if (is_key_column_) out << term_->grey();
  _print_aligned_value(out, data_[i].str());
  if (is_key_column_) out << term_->reset();
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
  xassert(n < 10000);
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


bool Data_TextColumn::_needs_escaping(const CString& str) {
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


std::string Data_TextColumn::_escape_string(const CString& str) {
  ostringstream out;
  bool allow_unicode = term_->unicode_allowed();

  size_t n = static_cast<size_t>(str.size);
  auto ch = reinterpret_cast<const unsigned char*>(str.ch);
  auto end = ch + n;
  while (ch < end) {
    auto c = *ch;
    // printable ASCII + UTF8 continuation bytes
    if ((c >= 0x20 && c <= 0x7E) || (c >= 0x80 && c <= 0xBF)) {
      out << c;
      ch++;
    }
    // C0 block + \x7F (DEL) character
    else if (c <= 0x1F || c == 0x7F) {
      out << term_->dim(_escaped_char(c));
      ch++;
    }
    // unicode start byte, when unicode output allowed
    else if (allow_unicode) {
      auto cc = ch[1];
      // C1 block still has to be escaped (codepoints 0x80 .. 0x9F, encoded
      // in utf8 as 2-byte sequences C2 80 .. C2 9F)
      if (c == 0xC2 && cc >= 0x80 && cc <= 0x9F) {
        out << term_->dim(_escaped_char(cc));
        ch += 2;
      } else {
        out << c;
        ch++;
      }
    }
    // unicode start byte, the codepoint needs to be hex-escaped
    else {
      int codepoint = read_codepoint_from_utf8(&ch);  // advances `ch`
      out << term_->dim(_escape_unicode(codepoint));
    }
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
      // if (is_key_column_) {
      //   rendered_value = sstring(term_->grey(rendered_value.str()));
      // }
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

void VSep_TextColumn::print_name(ostringstream& out) const {
  out << term_->reset() << term_->grey("|") << term_->bold();
}

void VSep_TextColumn::print_separator(ostringstream& out) const {
  out << '+';
}

void VSep_TextColumn::print_value(ostringstream& out, size_t) const {
  out << term_->grey("|");
}




}  // namespace dt
