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
import re
from collections import namedtuple



#-------------------------------------------------------------------------------
# Text repr
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
               "---+ --  --  --  ----  --  --  -----\n"
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
            "---+ --\n"
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
            "---+ --\n"
            " 0 | NA\n"
            " 1 | NA\n"
            " 2 | NA\n"
            " 3 | NA\n"
            " 4 | NA\n"
            "\n"
            "[5 rows x 1 column]\n"
            == str(DT))



#-------------------------------------------------------------------------------
# HTML repr
#-------------------------------------------------------------------------------

def parse_html_repr(html):
    # Here `re.S` means "single-line mode", i.e. allow '.' to match any
    # character, including the newline (by default '.' does not match '\n').
    mm = re.search("<div class='datatable'>(.*)</div>", html, re.S)
    html = mm.group(1).strip()
    mm = re.match(r"<table class='frame'>(.*)</table>\s*"
                  r"<div class='footer'>(.*)</div>", html, re.S)
    frame = mm.group(1).strip()
    footer = mm.group(2).strip()
    mm = re.match(r"<div class='frame_dimensions'>"
                  r"(\d+) rows? &times; (\d+) columns?</div>", footer, re.S)
    shape = (int(mm.group(1).strip()), int(mm.group(2).strip()))
    mm = re.match(r"<thead>(.*)</thead>\s*<tbody>(.*)</tbody>", frame, re.S)
    thead = mm.group(1).strip()
    tbody = mm.group(2).strip()
    mm = re.match("<tr class='colnames'><td class='row_index'></td>(.*)</tr>"
                  "\\s*"
                  "<tr class='coltypes'><td class='row_index'></td>(.*)</tr>",
                  thead, re.S)
    str_colnames = mm.group(1).strip()
    str_coltypes = mm.group(2).strip()
    colnames = re.findall("<th>(.*?)</th>", str_colnames)
    coltypes = re.findall("<td class='\\w+' title='(\\w+)'>", str_coltypes)
    str_rows = re.findall("<tr>(.*?)</tr>", tbody, re.S)
    rows = []
    for str_row in str_rows:
        row = re.findall("<td>(.*?)</td>", str_row, re.S)
        for i, elem in enumerate(row):
            if elem == "<span class=na>NA</span>":
                row[i] = None
        rows.append(row)
    html_repr = namedtuple("html_repr", ["names", "stypes", "shape", "data"])
    return html_repr(names=tuple(colnames),
                     stypes=tuple(dt.stype(s) for s in coltypes),
                     shape=shape,
                     data=rows)


def test_html_repr():
    DT = dt.Frame(A=range(5))
    html = DT._repr_html_()
    hr = parse_html_repr(html)
    assert hr.names == DT.names
    assert hr.stypes == DT.stypes
    assert hr.shape == DT.shape
    assert hr.data == [["0"], ["1"], ["2"], ["3"], ["4"]]


def test_html_repr_slice():
    DT = dt.Frame(A=range(5))[::-1, :]
    html = DT._repr_html_()
    hr = parse_html_repr(html)
    assert hr.names == DT.names
    assert hr.stypes == DT.stypes
    assert hr.shape == DT.shape
    assert hr.data == [["4"], ["3"], ["2"], ["1"], ["0"]]


def test_html_repr_unicode():
    src = "用起来还是很不稳定。很多按键都要点好几次才行。" * 2  # len=46
    DT = dt.Frame(U=[src[:n+1] for n in range(len(src))])
    html = DT._repr_html_()
    # Check that the string didn't get truncated
    assert src in html


def test_html_repr_joined_frame():
    L_dt = dt.Frame([[5, 6, 7, 9], [7, 8, 9, 10]], names = ["A", "B"])
    R_dt = dt.Frame([[5, 7], [7, 9], [1, 2]], names = ["A", "B", "yhat"])
    R_dt.key = ["A", "B"]
    DT = L_dt[:, :, dt.join(R_dt)]
    html = DT._repr_html_()
    hr = parse_html_repr(html)
    assert hr.names == ("A", "B", "yhat")
    assert hr.shape == (4, 3)
    assert hr.data == [['5', '7', '1'],
                       ['6', '8', None],
                       ['7', '9', '2'],
                       ['9', '10', None]]
