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
import pytest

bold = lambda s: "\x1b[1m" + s + "\x1b[m"
dim = lambda s: "\x1b[2m" + s + "\x1b[m"
grey = lambda s: "\x1b[90m" + s + "\x1b[m"
vsep = grey('|')
na = dim('NA')



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

    assert out == (
        bold('   ') + vsep + bold(" int  str   ") + "\n" +
        grey("-- + ---  ------") + "\n" +
        grey(' 0 ') + vsep + "   2  cogito\n" +
        grey(' 1 ') + vsep + "   7  ergo  \n" +
        grey(' 2 ') + vsep + "   0  sum   \n" +
        grey(' 3 ') + vsep + "   0  " + na + "    \n\n" +
        dim('[4 rows x 2 columns]') + "\n")


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
    assert out == (
        bold(' A  B  ') + vsep + bold("    C") + "\n" +
        grey("--  -- + ----") + "\n" +
        " 1  " + na + " " + vsep + "  3.2\n" +
        " 1  a  "         + vsep + " 14.1\n" +
        " 2  d  "         + vsep + " -7.7\n" +
        "\n" + dim('[3 rows x 3 columns]') + "\n")
