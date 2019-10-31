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
#include "utils/terminal/tstring.h"
#include "utils/terminal/terminal.h"
#include "utils/terminal/terminal_stream.h"
#include "column.h"
namespace dt {
using std::size_t;
using std::ostringstream;
using intvec = std::vector<size_t>;
using sstrvec = std::vector<tstring>;

static constexpr size_t NA_index = size_t(-1);



/**
  * Base class for a single column in TerminalWidget. The column has
  * to be pre-rendered before it can be properly layed out.
  *
  * `width_` is the computed width of the column, measured in terminal
  * columns (i.e. how many characters it will visually occupy after
  * being printed to the console). This property doesn't include
  * column's margins.
  *
  */
class TextColumn {
  protected:
    size_t  width_;
    bool    align_right_;
    bool    margin_left_;
    bool    margin_right_;
    size_t : 40;

    static const Terminal* term_;
    static tstring ellipsis_;
    static tstring na_value_;
    static tstring true_value_;
    static tstring false_value_;

  public:
    static void setup(const Terminal*);

    TextColumn();
    TextColumn(const TextColumn&) = default;
    TextColumn(TextColumn&&) noexcept = default;
    virtual ~TextColumn();

    void unset_left_margin();
    void unset_right_margin();
    int get_width() const;

    virtual void print_name(TerminalStream&) const = 0;
    virtual void print_separator(TerminalStream&) const = 0;
    virtual void print_value(TerminalStream&, size_t i) const = 0;
};



class Data_TextColumn : public TextColumn {
  private:
    sstrvec data_;
    tstring name_;
    int max_width_;
    int : 32;

  public:
    Data_TextColumn(const std::string& name,
                    const Column& col,
                    const intvec& indices,
                    int max_width);
    Data_TextColumn(const Data_TextColumn&) = default;
    Data_TextColumn(Data_TextColumn&&) noexcept = default;

    void print_name(TerminalStream&) const override;
    void print_separator(TerminalStream&) const override;
    void print_value(TerminalStream&, size_t i) const override;

  private:
    void _render_all_data(const Column& col, const intvec& indices);
    void _print_aligned_value(TerminalStream&, const tstring& value) const;
    void _align_at_dot();

    tstring _render_value(const Column&, size_t i) const;
    template <typename T>
    tstring _render_value_float(const Column&, size_t i) const;
    template <typename T>
    tstring _render_value_int(const Column&, size_t i) const;
    tstring _render_value_bool(const Column&, size_t i) const;
    tstring _render_value_string(const Column&, size_t i) const;

    bool _needs_escaping(const CString&) const;
    tstring _escape_string(const CString&) const;
};



class VSep_TextColumn : public TextColumn {
  public:
    VSep_TextColumn();
    VSep_TextColumn(const VSep_TextColumn&) = default;
    VSep_TextColumn(VSep_TextColumn&&) noexcept = default;

    void print_name(TerminalStream&) const override;
    void print_separator(TerminalStream&) const override;
    void print_value(TerminalStream&, size_t i) const override;
};



class Ellipsis_TextColumn : public TextColumn {
  private:
    tstring ell_;

  public:
    Ellipsis_TextColumn();
    Ellipsis_TextColumn(const Ellipsis_TextColumn&) = default;
    Ellipsis_TextColumn(Ellipsis_TextColumn&&) noexcept = default;

    void print_name(TerminalStream&) const override;
    void print_separator(TerminalStream&) const override;
    void print_value(TerminalStream&, size_t i) const override;
};



}  // namespace dt
#endif
