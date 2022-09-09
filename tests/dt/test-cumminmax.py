#!/usr/bin/env python
# -*- coding: utf-8 -*-
#-------------------------------------------------------------------------------
# Copyright 2022 H2O.ai
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
import math
import pytest
from datatable import dt, f, cummax, cummin, FExpr, by
from tests import assert_equals


#-------------------------------------------------------------------------------
# Errors
#-------------------------------------------------------------------------------

@pytest.mark.parametrize("mm", [cummin, cummax])
def test_cumminmax_non_numeric(mm):
    DT = dt.Frame(list('abcde'))
    with pytest.raises(TypeError,
                       match = r'Invalid column of type str32 in ' + mm.__name__):
        DT[:, mm(f[0])]


@pytest.mark.parametrize("mm", [cummin, cummax])
def test_cumminmax_non_numeric_by(mm):
    DT = dt.Frame(list('abcde'))
    with pytest.raises(TypeError,
                       match = r'Invalid column of type str32 in ' + mm.__name__):
        DT[:, mm(f[0]), by(f[0])]


@pytest.mark.parametrize("mm", [cummin, cummax])
def test_cumminmax_no_argument(mm):
    msg = (f"Function datatable.{mm.__name__}"
           "\\(\\) requires exactly 1 positional argument, but none were given")
    with pytest.raises(TypeError, match = msg):
        mm()


#-------------------------------------------------------------------------------
# Normal
#-------------------------------------------------------------------------------

@pytest.mark.parametrize("mm", [cummin, cummax])
def test_cumminmax_str(mm):
  assert str(mm(f.A)) == "FExpr<" + mm.__name__ + "(f.A)>"
  assert str(mm(f.A) + 1) == "FExpr<" + mm.__name__ + "(f.A) + 1>"
  assert str(mm(f.A + f.B)) == "FExpr<" + mm.__name__ + "(f.A + f.B)>"
  assert str(mm(f.B)) == "FExpr<" + mm.__name__ + "(f.B)>"
  assert str(mm(f[:2])) == "FExpr<"+ mm.__name__ + "(f[:2])>"


@pytest.mark.parametrize("mm", [cummin, cummax])
def test_cumminmax_empty_frame(mm):
    DT = dt.Frame()
    expr_mm = mm(DT)
    assert isinstance(expr_mm, FExpr)
    assert_equals(DT[:, mm(f[:])], DT)


@pytest.mark.parametrize("mm", [cummin, cummax])
def test_cumminmax_void(mm):
    DT = dt.Frame([None, None, None])
    DT_mm = DT[:, mm(f[:])]
    assert_equals(DT_mm, DT)


@pytest.mark.parametrize("mm", [cummin, cummax])
def test_cumminmax_trivial(mm):
    DT = dt.Frame([0]/dt.int64)
    mm_fexpr = mm(f[:])
    DT_mm = DT[:, mm_fexpr]
    assert isinstance(mm_fexpr, FExpr)
    assert_equals(DT, DT_mm)


def test_cumminmax_bool():
    DT = dt.Frame([None, False, None, True, False, True])
    DT_mm = DT[:, [cummin(f[:]), cummax(f[:])]]
    DT_ref = dt.Frame([
                [None, False, False, False, False, False],
                [None, False, False, True, True, True]
             ])
    assert_equals(DT_mm, DT_ref)


def test_cumminmax_small():
    DT = dt.Frame([range(5), [None, -1, None, 5.5, 3]])
    DT_mm = DT[:, [cummin(f[:]), cummax(f[:])]]
    DT_ref = dt.Frame([
                 [0, 0, 0, 0, 0]/dt.int32,
                 [None, -1, -1, -1, -1]/dt.float64,
                 [0, 1, 2, 3, 4],
                 [None, -1, -1, 5.5, 5.5]
             ])
    assert_equals(DT_mm, DT_ref)


def test_cumminmax_dates():
    from datetime import date as d
    src = [None, d(1997, 9, 1), d(2002, 7, 31), None, d(2000, 2, 20)]
    DT = dt.Frame(src)
    DT_mm = DT[:, [cummin(f.C0), cummax(f.C0)]]
    DT_ref = dt.Frame([
                 [None, d(1997, 9, 1), d(1997, 9, 1), d(1997, 9, 1), d(1997, 9, 1)],
                 [None, d(1997, 9, 1), d(2002, 7, 31), d(2002, 7, 31), d(2002, 7, 31)],
             ])
    assert_equals(DT_mm, DT_ref)


def test_cumminmax_times():
    from datetime import datetime as d
    src = [None, d(1997, 9, 1, 10, 11, 5), d(1997, 9, 1, 10, 10, 5), None, None]
    DT = dt.Frame(src)
    DT_mm = DT[:, [cummin(f.C0), cummax(f.C0)]]
    DT_ref = dt.Frame([
                 [None,
                  d(1997, 9, 1, 10, 11, 5),
                  d(1997, 9, 1, 10, 10, 5),
                  d(1997, 9, 1, 10, 10, 5),
                  d(1997, 9, 1, 10, 10, 5)],
                 [None,
                  d(1997, 9, 1, 10, 11, 5),
                  d(1997, 9, 1, 10, 11, 5),
                  d(1997, 9, 1, 10, 11, 5),
                  d(1997, 9, 1, 10, 11, 5)],
             ])
    assert_equals(DT_mm, DT_ref)


def test_cumminmax_groupby():
    DT = dt.Frame([[2, 1, 1, 1, 2], [1.5, -1.5, math.inf, None, 3]])
    DT_mm = DT[:, [cummin(f[:]), cummax(f[:])], by(f[0])]
    DT_ref = dt.Frame([
                 [1, 1, 1, 2, 2],
                 [-1.5, -1.5, -1.5, 1.5, 1.5],
                 [-1.5, math.inf, math.inf, 1.5, 3]
             ])
    assert_equals(DT_mm, DT_ref)


def test_cumminmax_grouped_column():
    DT = dt.Frame([2, 1, None, 1, 2])
    DT_mm = DT[:, [cummin(f[0]), cummax(f[0])], by(f[0])]
    DT_ref = dt.Frame([
                [None, 1, 1, 2, 2],
                [None, 1, 1, 2, 2],
                [None, 1, 1, 2, 2]
             ])
    assert_equals(DT_mm, DT_ref)


def test_cumminmax_groupby_complex():
    DT = dt.Frame([[3, 14, 15, 92, 6], ["a", "cat", "a", "dog", "cat"]])
    DT_mm = DT[:, [cummin(f[0].min()), cummax(f[0].max())], by(f[1])]

    DT_ref = dt.Frame(
                 {"C1" : ["a", "a", "cat", "cat", "dog"],
                 "C0" : [3, 3, 6, 6, 92],
                 "C2" : [15, 15, 14, 14, 92]}
             )
    assert_equals(DT_mm, DT_ref)


