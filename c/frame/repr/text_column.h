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
#ifndef dt_FRAME_REPR_TEXT_COLUMN_h
#define dt_FRAME_REPR_TEXT_COLUMN_h
#include <iostream>
#include <string>
#include <vector>
#include "frame/repr/sstring.h"
#include "utils/terminal.h"
#include "column.h"
namespace dt {
using std::size_t;
using std::ostringstream;
using intvec = std::vector<size_t>;
using sstrvec = std::vector<sstring>;

static constexpr size_t NA_index = size_t(-1);



class TextColumn {
  private:
    sstrvec data_;
    sstring name_;
    size_t  width_;
    bool    align_right_;
    bool    margin_left_;
    bool    margin_right_;
    bool    border_right_;
    bool    is_key_column_;
    int : 24;

    static const Terminal* term_;
    static sstring ellipsis_;
    static sstring na_value_;

  public:
    static void setup(const Terminal*);

    TextColumn(const std::string& name,
               const Column& col,
               const intvec& indices,
               bool is_key_column = false);
    TextColumn(const TextColumn&) = default;
    TextColumn(TextColumn&&) = default;

    void unset_left_margin();
    void unset_right_margin();
    void set_right_border();

    void print_name(ostringstream&) const;
    void print_separator(ostringstream&) const;
    void print_value(ostringstream&, size_t i) const;

  private:
    void _render_all_data(const Column& col, const intvec& indices);
    void _print_aligned_value(ostringstream&, const sstring& value) const;
    void _print_whitespace(ostringstream&, size_t n) const;
    void _align_at_dot();

    sstring _render_value(const Column&, size_t i) const;
    template <typename T>
    sstring _render_value_float(const Column& col, size_t i) const;
    template <typename T>
    sstring _render_value_int(const Column& col, size_t i) const;
    sstring _render_value_bool(const Column&, size_t i) const;
    sstring _render_value_string(const Column&, size_t i) const;
};





}  // namespace dt
#endif
