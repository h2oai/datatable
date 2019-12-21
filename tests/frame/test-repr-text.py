#!/usr/bin/env python
# -*- coding: utf-8 -*-
#-------------------------------------------------------------------------------
# Copyright 2018-2019 H2O.ai
#
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
# IN THE SOFTWARE.
#-------------------------------------------------------------------------------
import datatable as dt
import os
import pytest
import re
import subprocess
import sys


def color_line(s):
    return re.sub(r"((?:…|~|NA|\\n|\\r|\\t|\\x..|\\u....|\\U000.....)+)",
                  "\x1b[2m\\1\x1b[0m", s)


def color_header(s):
    return re.sub(r"((?:NA|\\n|\\r|\\t|\\x..|\\u....|\\U000.....)+)",
                  "\x1b[2m\\1\x1b[0;1m",
                  re.sub("…", "\x1b[0;2m…\x1b[0;1m", s))


def check_colored_output(actual, header, separator, *body, keyed=False):
    header1, header2 = color_header(header).split('|', 2)
    footer = body[-1]
    out = ''
    out += "\x1b[1m" + header1
    out += "\x1b[0;90m" + "|"
    out += "\x1b[0;1m" + header2 + "\x1b[0m" + '\n'
    out += "\x1b[90m" + separator + "\x1b[0m" + '\n'
    for line in body[:-1]:
        line1, line2 = color_line(line).split('|', 2)
        if keyed:
            out += line1 + "\x1b[90m|"
        else:
            out += "\x1b[90m" + line1 + '|'
        out += "\x1b[0m" + line2 + '\n'
    out += '\n'
    out += "\x1b[2m" + footer + "\x1b[0m\n"
    if out != actual:
        raise AssertionError("Invalid Frame display:\n"
                             "expected = %r\n"
                             "actual   = %r\n" % (out, actual))



#-------------------------------------------------------------------------------
# Tests
#-------------------------------------------------------------------------------

def test_dt_view(capsys):
    dt0 = dt.Frame([
        [2, 7, 0, 0],
        [True, False, False, True],
        [1, 1, 1, 1],
        [0.1, 2, -4, 4.4],
        [None, None, None, None],
        [0, 0, 0, 0],
        ["1", "2", "hello", "world"],
    ], names=list("ABCDEFG"))
    dt0_str = ("   |  A   B   C     D   E   F  G    \n"
               "-- + --  --  --  ----  --  --  -----\n"
               " 0 |  2   1   1   0.1  NA   0  1    \n"
               " 1 |  7   0   1   2    NA   0  2    \n"
               " 2 |  0   0   1  -4    NA   0  hello\n"
               " 3 |  0   1   1   4.4  NA   0  world\n"
               "\n"
               "[4 rows x 7 columns]\n")
    dt0.view(interactive=False, plain=True)
    out, err = capsys.readouterr()
    assert not err
    assert out == dt0_str
    assert str(dt0) == dt0_str


def test_dt_view_keyed(capsys):
    DT = dt.Frame(A=range(5), B=list("cdbga"))
    DT.key = "B"
    DT.view(interactive=False, plain=True)
    out, err = capsys.readouterr()
    assert not err
    assert ("B  |  A\n"
            "-- + --\n"
            "a  |  4\n"
            "b  |  2\n"
            "c  |  0\n"
            "d  |  1\n"
            "g  |  3\n"
            "\n"
            "[5 rows x 2 columns]\n"
            in out)


def test_long_frame():
    DT = dt.Frame(A=["A%03d" % (i+1) for i in range(200)])
    assert dt.options.display.max_nrows < 100
    assert dt.options.display.head_nrows == 15
    assert dt.options.display.tail_nrows == 5
    assert str(DT) == (
        "    | A   \n"
        "--- + ----\n"
        "  0 | A001\n"
        "  1 | A002\n"
        "  2 | A003\n"
        "  3 | A004\n"
        "  4 | A005\n"
        "  5 | A006\n"
        "  6 | A007\n"
        "  7 | A008\n"
        "  8 | A009\n"
        "  9 | A010\n"
        " 10 | A011\n"
        " 11 | A012\n"
        " 12 | A013\n"
        " 13 | A014\n"
        " 14 | A015\n"
        "  … | …   \n"
        "195 | A196\n"
        "196 | A197\n"
        "197 | A198\n"
        "198 | A199\n"
        "199 | A200\n"
        "\n"
        "[200 rows x 1 column]\n")


def test_str_after_resize():
    # See issue #1527
    DT = dt.Frame(A=[])
    DT.nrows = 5
    assert ("   |  A\n"
            "-- + --\n"
            " 0 | NA\n"
            " 1 | NA\n"
            " 2 | NA\n"
            " 3 | NA\n"
            " 4 | NA\n"
            "\n"
            "[5 rows x 1 column]\n"
            == str(DT))


def test_str_unicode():
    DT = dt.Frame([["møøse"], ["𝔘𝔫𝔦𝔠𝔬𝔡𝔢"], ["J̲o̲s̲é̲"], ["🚑🐧💚💥✅"]])
    assert str(DT) == (
        "   | C0     C1       C2    C3        \n"
        "-- + -----  -------  ----  ----------\n"
        " 0 | møøse  𝔘𝔫𝔦𝔠𝔬𝔡𝔢  J̲o̲s̲é̲  🚑🐧💚💥✅\n"
        "\n"
        "[1 row x 4 columns]\n")


def test_str_sanitize():
    DT = dt.Frame([
        ["понеділок", "вівторок", "середа", "четвер", "п'ятниця",
         "субота", "неділя"],
        [3, 15, None, 77, -444, 0, 55],
        [None, "Ab\ncd", "\x00\x01\x02\x03\x04", "one\ttwo", "365",
         "🎁", "the end."],
        ["|"] * 7
    ], names=["тиждень", "numbers", "random", "*"])
    assert str(DT) == "\n".join([
        r"   | тиждень    numbers  random                * ",
        r"-- + ---------  -------  --------------------  --",
        r" 0 | понеділок        3  NA                    | ",
        r" 1 | вівторок        15  Ab\ncd                | ",
        r" 2 | середа          NA  \x00\x01\x02\x03\x04  | ",
        r" 3 | четвер          77  one\ttwo              | ",
        r" 4 | п'ятниця      -444  365                   | ",
        r" 5 | субота           0  🎁                    | ",
        r" 6 | неділя          55  the end.              | ",
        r"",
        r"[7 rows x 4 columns]",
        r""
    ])


def test_str_sanitize_C0():
    DT = dt.Frame(C0=[chr(i) for i in range(32)])
    with dt.options.context(**{"display.max_nrows": 40}):
        assert str(DT) == "".join(
            ["   | C0  \n",
             "-- + ----\n"] +
            [" 9 | \\t  \n" if i == 9 else
             "10 | \\n  \n" if i == 10 else
             "13 | \\r  \n" if i == 13 else
             "%2d | \\x%02X\n" % (i, i)
             for i in range(32)] +
            ["\n[32 rows x 1 column]\n"])


def test_str_sanitize_C1():
    DT = dt.Frame(C1=[chr(i) for i in range(0x7F, 0xA0)])
    with dt.options.context(**{"display.max_nrows": 40}):
        assert str(DT) == "".join(
            ["   | C1  \n",
             "-- + ----\n"] +
            ["%2d | \\x%2X\n" % (i - 0x7F, i)
             for i in range(0x7F, 0xA0)] +
            ["\n[33 rows x 1 column]\n"])


def test_tamil_script():
    DT = dt.Frame([["உங்களுக்கு வணக்கம்",
                    "நீங்கள் எப்படி இருக்கிறீர்கள்?",
                    "நல்வரவு"],
                   ["#"] * 3],
                  names=["தமிழ் வாக்கியங்கள்", "#"])
    # Despite how it might look in a text editor, these align well in a console
    assert str(DT) == (
        "   | தமிழ் வாக்கியங்கள்         # \n"
        "-- + -------------------  --\n"
        " 0 | உங்களுக்கு வணக்கம்         # \n"
        " 1 | நீங்கள் எப்படி இருக்கிறீர்கள்?  # \n"
        " 2 | நல்வரவு                # \n"
        "\n[3 rows x 2 columns]\n")


def test_chinese():
    DT = dt.Frame([["蒙蒂·蟒蛇",
                    "小洞不补，大洞吃苦",
                    "風向轉變時,有人築牆,有人造風車",
                    "師傅領進門，修行在個人"],
                   ["#"] * 4],
                  names=["中文", "#"])
    assert str(DT) == (
        "   | 中文                            # \n"
        "-- + ------------------------------  --\n"
        " 0 | 蒙蒂·蟒蛇                       # \n"
        " 1 | 小洞不补，大洞吃苦              # \n"
        " 2 | 風向轉變時,有人築牆,有人造風車  # \n"
        " 3 | 師傅領進門，修行在個人          # \n"
        "\n[4 rows x 2 columns]\n")


def test_colored_output(capsys):
    DT = dt.Frame([[2, 7, 0, 0],
                   ["cogito", "ergo", "sum", None]],
                  names=["int", "str"])
    assert dt.options.display.use_colors == True
    DT.view(interactive=False)
    out, err = capsys.readouterr()
    assert not err
    check_colored_output(out,
        "   | int  str   ",
        "-- + ---  ------",
        " 0 |   2  cogito",
        " 1 |   7  ergo  ",
        " 2 |   0  sum   ",
        " 3 |   0  NA    ",
        "[4 rows x 2 columns]")


def test_option_use_colors(capsys):
    DT = dt.Frame(A=range(4))
    with dt.options.context(**{"display.use_colors": False}):
        DT.view(interactive=False)
        out, err = capsys.readouterr()
        assert err == ''
        assert out == (
            "   |  A\n"
            "-- + --\n"
            " 0 |  0\n"
            " 1 |  1\n"
            " 2 |  2\n"
            " 3 |  3\n"
            "\n"
            "[4 rows x 1 column]\n")


def test_colored_keyed(capsys):
    DT = dt.Frame(A=[1, 2, 1], B=[None, 'd', 'a'], C=[3.2, -7.7, 14.1])
    DT.key = ('A', 'B')
    DT.view(interactive=False)
    out, err = capsys.readouterr()
    assert not err
    check_colored_output(out,
        " A  B  |    C",
        "--  -- + ----",
        " 1  NA |  3.2",
        " 1  a  | 14.1",
        " 2  d  | -7.7",
        "[3 rows x 3 columns]",
        keyed=True)


def test_colored_escaped_name(capsys):
    DT = dt.Frame(names=["#(\x80)#"])  # U+0080 is always escaped in the output
    DT.view(interactive=False)
    out, err = capsys.readouterr()
    assert not err
    check_colored_output(out,
        "   | #(\\x80)#",
        "-- + --------",
        "[0 rows x 1 column]")


def test_horizontal_elision(capsys):
    DT = dt.Frame([["1234567890" * 3]] * 20)
    with dt.options.display.context(allow_unicode=True, use_colors=True):
        DT.view(interactive=False)
    out, err = capsys.readouterr()
    assert not err
    if os.environ.get("TRAVIS"):
        # On Travis the output is truncated to 80 width
        check_colored_output(out,
            "   | C0                              C1        …  C19                           ",
            "-- + ------------------------------  --------     ------------------------------",
            " 0 | 123456789012345678901234567890  1234567…  …  123456789012345678901234567890",
            "[1 row x 20 columns]"
            )
    else:
        # Normally the output is truncated to 120 width (default terminal width for non-tty)
        check_colored_output(out,
            "   | C0                              C1                              C2                …  C19                           ",
            "-- + ------------------------------  ------------------------------  ----------------     ------------------------------",
            " 0 | 123456789012345678901234567890  123456789012345678901234567890  123456789012345…  …  123456789012345678901234567890",
            "[1 row x 20 columns]")






#-------------------------------------------------------------------------------
# dt.options.display.max_nrows
#-------------------------------------------------------------------------------

def test_option_allow_unicode(capsys):
    DT = dt.Frame(uni=["møøse", "𝔘𝔫𝔦𝔠𝔬𝔡𝔢", "J̲o̲s̲é̲", "🚑💥✅"])
    with dt.options.display.context(allow_unicode=False):
        DT.view(interactive=False)
    out, err = capsys.readouterr()
    assert not err
    check_colored_output(out,
        "   | uni" + ' '*67,
        "-- + ---" + '-'*67,
        " 0 | m" + "\\xF8"*2 + "se" + " "*59,
        " 1 | " + "".join("\\U%08X" % ord(c) for c in '𝔘𝔫𝔦𝔠𝔬𝔡𝔢'),
        " 2 | " + "\\u0332".join(["J", "o", "s", "\\xE9", ""]) + " "*39,
        " 3 | \\U0001F691\\U0001F4A5\\u2705" + " "*44,
        "[4 rows x 1 column]")


def test_option_allow_unicode_long_frame(capsys):
    DT = dt.Frame(A=range(100))
    with dt.options.display.context(allow_unicode=False, use_colors=False):
        DT.view(interactive=False)
    out, err = capsys.readouterr()
    assert not err
    assert out == (
        "    |   A\n"
        "--- + ---\n" +
        "".join(" %2d |  %2d\n" % (i, i) for i in range(15)) +
        "... | ...\n" +
        "".join(" %2d |  %2d\n" % (i, i) for i in range(95, 100)) +
        "\n" +
        "[100 rows x 1 column]\n")


def test_allow_unicode_column_name(capsys):
    # See issue #2118
    DT = dt.Frame(names=["тест"])
    with dt.options.display.context(allow_unicode=False, use_colors=False):
        DT.view(interactive=False)
    out, err = capsys.readouterr()
    assert not err
    assert out == (
        "   | \\u0442\\u0435\\u0441\\u0442\n"
        "-- + ------------------------\n"
        "\n[0 rows x 1 column]\n")




#-------------------------------------------------------------------------------
# dt.options.display.max_nrows
#-------------------------------------------------------------------------------

def test_max_nrows_large():
    # Using large `max_nrows` produces all rows of the frame
    DT = dt.Frame(A=["A%03d" % (i+1) for i in range(200)])
    with dt.options.display.context(max_nrows=None):
        assert str(DT) == (
            "    | A   \n" +
            "--- + ----\n" +
            "".join("%3d | %s\n" % (i, DT[i, 0]) for i in range(200)) +
            "\n" +
            "[200 rows x 1 column]\n")


def test_max_nrows_exact():
    # Check that if a frame has more than `max_nrows` rows it gets truncated,
    # but if it has equal or less than `max_nrows` then it is displayed fully.
    DT = dt.Frame(R=range(17))
    with dt.options.display.context(head_nrows=1, tail_nrows=1, max_nrows=16):
        assert str(DT) == (
            "   |  R\n"
            "-- + --\n"
            " 0 |  0\n"
            " … |  …\n"
            "16 | 16\n"
            "\n[17 rows x 1 column]\n")

        assert str(DT[:-1, :]) == (
            "   |  R\n" +
            "-- + --\n" +
            "".join("%2d | %2d\n" % (i, i) for i in range(16)) +
            "\n[16 rows x 1 column]\n")



def test_max_nrows_small():
    # If `max_nrows` is set below `nht = head_nrows + tail_nrows`,
    # then any frame with less rows than `nht` will still be rendered
    # in full.
    DT = dt.Frame(A=range(5))
    assert dt.options.display.head_nrows + dt.options.display.tail_nrows > 5
    with dt.options.display.context(max_nrows=0):
        assert dt.options.display.max_nrows == 0
        assert str(DT) == (
            "   |  A\n"
            "-- + --\n"
            " 0 |  0\n"
            " 1 |  1\n"
            " 2 |  2\n"
            " 3 |  3\n"
            " 4 |  4\n"
            "\n[5 rows x 1 column]\n")


def test_max_nrows_negative():
    for t in [-1, -5, -1000000, None]:
        with dt.options.display.context(max_nrows=t):
            assert dt.options.display.max_nrows is None


def test_max_nrows_invalid():
    for t in [3.4, False, dt]:
        with pytest.raises(TypeError, match="display.max_nrows should be "
                                            "an integer"):
            dt.options.display.max_nrows = t




#-------------------------------------------------------------------------------
# dt.options.display.[head|tail]_nrows
#-------------------------------------------------------------------------------

def test_long_frame_head_tail():
    DT = dt.Frame(A=["A%03d" % (i+1) for i in range(200)])
    with dt.options.display.context(head_nrows=5, tail_nrows=3):
        assert str(DT) == (
            "    | A   \n"
            "--- + ----\n"
            "  0 | A001\n"
            "  1 | A002\n"
            "  2 | A003\n"
            "  3 | A004\n"
            "  4 | A005\n"
            "  … | …   \n"
            "197 | A198\n"
            "198 | A199\n"
            "199 | A200\n"
            "\n"
            "[200 rows x 1 column]\n")


def test_small_head_tail():
    DT = dt.Frame(boo=range(10))
    with dt.options.display.context(head_nrows=1, tail_nrows=1, max_nrows=1):
        assert str(DT) == (
            "   | boo\n"
            "-- + ---\n"
            " 0 |   0\n"
            " … |   …\n"
            " 9 |   9\n"
            "\n"
            "[10 rows x 1 column]\n")


def test_head_0():
    DT = dt.Frame(T1=range(100))
    with dt.options.display.context(head_nrows=0, tail_nrows=3):
        assert str(DT) == (
            "   | T1\n"
            "-- + --\n"
            " … |  …\n"
            "97 | 97\n"
            "98 | 98\n"
            "99 | 99\n"
            "\n"
            "[100 rows x 1 column]\n")


def test_tail_0():
    DT = dt.Frame(T2=range(100))
    with dt.options.display.context(head_nrows=3, tail_nrows=0):
        assert str(DT) == (
            "   | T2\n"
            "-- + --\n"
            " 0 |  0\n"
            " 1 |  1\n"
            " 2 |  2\n"
            " … |  …\n"
            "\n"
            "[100 rows x 1 column]\n")


def test_headtail_0():
    DT = dt.Frame(T3=range(100))
    with dt.options.display.context(head_nrows=0, tail_nrows=0):
        assert str(DT) == (
            "   | T3\n"
            "-- + --\n"
            " … |  …\n"
            "\n"
            "[100 rows x 1 column]\n")


@pytest.mark.parametrize('opt', ['head_nrows', 'tail_nrows'])
def test_headtail_nrows_invalid(opt):
    with pytest.raises(ValueError, match="display.%s cannot be negative" % opt):
        setattr(dt.options.display, opt, -3)

    for t in [3.4, False, dt]:
        with pytest.raises(TypeError, match="display.%s should be "
                                            "an integer" % opt):
            setattr(dt.options.display, opt, t)



#-------------------------------------------------------------------------------
# dt.options.display.max_column_width
#-------------------------------------------------------------------------------

def test_max_width_name():
    assert dt.options.display.max_column_width == 100
    DT = dt.Frame(names=["a" * 1234])
    assert str(DT) == (
        "   | " + "a" * 99 + "…\n" +
        "-- + " + "-" * 100 + "\n" +
        "\n[0 rows x 1 column]\n")


def test_max_width_data():
    DT = dt.Frame(A=['foo', None, 'bazinga', '', '12345'])
    with dt.options.display.context(max_column_width=5):
        assert str(DT) == (
            "   | A    \n"
            "-- + -----\n"
            " 0 | foo  \n"
            " 1 | NA   \n"
            " 2 | bazi…\n"
            " 3 |      \n"
            " 4 | 12345\n"
            "\n[5 rows x 1 column]\n")


@pytest.mark.parametrize('uni', [True, False])
def test_max_width_colored(capsys, uni):
    DT = dt.Frame(S=['abcdefg'])
    with dt.options.display.context(max_column_width=4, allow_unicode=uni):
        DT.view(interactive=False)
        out, err = capsys.readouterr()
        symbol = "…" if uni else "~"
        assert not err
        check_colored_output(out,
            "   | S   ",
            "-- + ----",
            " 0 | abc" + symbol,
            "[1 row x 1 column]")


def test_max_width1():
    DT = dt.Frame(A=['foo', None, 'z'])
    with dt.options.display.context(max_column_width=1):
        assert dt.options.display.max_column_width == 2
        dt.options.display.max_column_width = 0
        assert dt.options.display.max_column_width == 2
        assert str(DT) == (
            "   | A \n"
            "-- + --\n"
            " 0 | f…\n"
            " 1 | NA\n"
            " 2 | z \n"
            "\n[3 rows x 1 column]\n")


def test_max_width_unicode():
    # A unicode string consists of 3 emoji characters, however all
    # characters are double-width, so the string is effectively
    # 6 terminal places in size.
    DT = dt.Frame(A=["👽👽👽"])

    # The width of 6 is just enough to fit the entire string
    with dt.options.display.context(max_column_width=6):
        assert str(DT) == (
            "   | A     \n"
            "-- + ------\n"
            " 0 | 👽👽👽\n"
            "\n[1 row x 1 column]\n")

    # Under the width of 5 the last character doesn't fit, so it is
    # replaced with a single-width ellipsis character
    with dt.options.display.context(max_column_width=5):
        assert str(DT) == (
            "   | A    \n"
            "-- + -----\n"
            " 0 | 👽👽…\n"
            "\n[1 row x 1 column]\n")

    # If max_column_width is 4 then we must truncate after the first
    # character, since truncating after the second and adding the
    # ellipsis makes the string of length 5.
    with dt.options.display.context(max_column_width=4):
        assert str(DT) == (
            "   | A  \n"
            "-- + ---\n"
            " 0 | 👽…\n"
            "\n[1 row x 1 column]\n")


def test_max_width_invalid():
    for t in [3.4, False, dt]:
        with pytest.raises(TypeError, match="display.max_column_width "
                                            "should be an integer"):
            dt.options.display.max_column_width = t



def test_max_width_none():
    with dt.options.display.context(max_column_width=None):
        assert dt.options.display.max_column_width is None
        dt.options.display.max_column_width = -1
        assert dt.options.display.max_column_width is None
        DT = dt.Frame(Long=["a" * 12321])
        assert str(DT) == (
            "   | Long" + " "*12317 + "\n" +
            "-- + " + "-" * 12321 + "\n" +
            " 0 | " + "a" * 12321 + "\n" +
            "\n[1 row x 1 column]\n")
    assert dt.options.display.max_column_width == 100


def test_max_width_nounicode(capsys):
    DT = dt.Frame(A=["👽👽"])
    with dt.options.display.context(use_colors=False, allow_unicode=False):
        with dt.options.display.context(max_column_width=10):
            DT.view(interactive=False)
            out, _ = capsys.readouterr()
            assert out == (
                "   | A \n"
                "-- + --\n"
                " 0 | ~ \n"
                "\n[1 row x 1 column]\n")

        with dt.options.display.context(max_column_width=15):
            DT.view(interactive=False)
            out, _ = capsys.readouterr()
            assert out == (
                "   | A          \n"
                "-- + -----------\n"
                " 0 | \\U0001F47D~\n"
                "\n[1 row x 1 column]\n")

        with dt.options.display.context(max_column_width=20):
            DT.view(interactive=False)
            out, _ = capsys.readouterr()
            assert out == (
                "   | A                   \n"
                "-- + --------------------\n"
                " 0 | \\U0001F47D\\U0001F47D\n"
                "\n[1 row x 1 column]\n")




#-------------------------------------------------------------------------------
# Misc
#-------------------------------------------------------------------------------

def test_encoding_autodetection(tempfile):
    # Check that if `sys.stdout` doesn't have UTF-8 encoding then
    # datatable can detect it and set display.allow_unicode option
    # to False automatically.
    cmd = ("import sys; " +
           "sys.stdout = open('%s', 'w', encoding='ascii'); " % tempfile +
           "import datatable as dt; " +
           "assert dt.options.display.allow_unicode is False; " +
           "DT = dt.Frame(A=['\\u2728']); " +
           "dt.options.display.use_colors = False; " +
           "DT.view(False)")
    out = subprocess.check_output([sys.executable, "-c", cmd])
    assert not out
    with open(tempfile, "r", encoding='ascii') as f:
        out = f.read()
        assert out == ("   | A     \n"
                       "-- + ------\n"
                       " 0 | \\u2728\n"
                       "\n[1 row x 1 column]\n")
