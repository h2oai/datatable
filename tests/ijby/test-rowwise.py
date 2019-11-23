#!/usr/bin/env python
# -*- coding: utf-8 -*-
#-------------------------------------------------------------------------------
# Copyright 2019 H2O.ai
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
import random
import datatable as dt
from datatable import f, g, stype, ltype, join
from datatable import rowall, rowany, rowsum, rowcount, rowmin, rowmax, \
                      rowfirst, rowlast, rowmean, rowsd
from datatable.internal import frame_integrity_check
from tests import list_equals, assert_equals, noop

stypes_int = ltype.int.stypes
stypes_float = ltype.real.stypes
stypes_str = ltype.str.stypes



#-------------------------------------------------------------------------------
# rowall()
#-------------------------------------------------------------------------------

def test_rowall_simple():
    DT = dt.Frame([[True, True, False, True, None, True],
                   [True, False, True, True, True, True],
                   [True, True,  True, True, True, True]])
    RES = DT[:, rowall(f[:])]
    assert_equals(RES, dt.Frame([True, False, False, True, False, True]))


def test_rowall_single_column():
    DT = dt.Frame([[True, False, None, True]])
    RES = rowall(DT)
    assert_equals(RES, dt.Frame([True, False, False, True]))


def test_rowall_sequence_of_columns():
    DT = dt.Frame(A=[True, False, True],
                  B=[3, 6, 1],
                  C=[True, None, False],
                  D=[True, True, True],
                  E=['ha', 'nope', None])
    DT = DT[:, list("ABCDE")]  # force correct column order for py3.5
    RES = DT[:, rowall(f.A, f["C":"D"])]
    assert_equals(RES, dt.Frame([True, False, False]))


def test_rowall_no_columns():
    DT = dt.Frame(A=[True, False, True, True, None])
    R1 = DT[:, rowall()]
    R2 = DT[:, rowall(f[str])]
    assert_equals(R1, dt.Frame([True]))
    assert_equals(R2, dt.Frame([True]))


@pytest.mark.parametrize('st', stypes_int + stypes_float + stypes_str)
def test_rowall_wrong_type(st):
    DT = dt.Frame(A=[], stype=st)
    with pytest.raises(TypeError, match="Function `rowall` requires a sequence "
                                        "of boolean columns"):
        assert DT[:, rowall(f.A)]




#-------------------------------------------------------------------------------
# rowany()
#-------------------------------------------------------------------------------

def test_rowany_simple():
    DT = dt.Frame([[True, True, False, False, None,  False],
                   [True, False, True, False, True,  None],
                   [True, True,  True, False, False, None]])
    RES = DT[:, rowany(f[:])]
    assert_equals(RES, dt.Frame([True, True, True, False, True, False]))


@pytest.mark.parametrize('st', stypes_int + stypes_float + stypes_str)
def test_rowany_wrong_type(st):
    DT = dt.Frame(A=[], stype=st)
    with pytest.raises(TypeError, match="Function `rowany` requires a sequence "
                                        "of boolean columns"):
        assert DT[:, rowany(f.A)]




#-------------------------------------------------------------------------------
# rowcount()
#-------------------------------------------------------------------------------

def test_rowcount_different_types():
    DT = dt.Frame([[1, 4, None, 7, 0, None],
                   [True, None, None, False, False, False],
                   [7.4, math.nan, None, math.inf, -math.inf, 1.6e300],
                   ["A", "", None, None, "NaN", "None"]])
    RES = DT[:, rowcount(f[:])]
    assert_equals(RES, dt.Frame([4, 2, 0, 3, 4, 3], stype=dt.int32))




#-------------------------------------------------------------------------------
# rowfirst(), rowlast()
#-------------------------------------------------------------------------------

def test_rowfirstlast_bools():
    DT = dt.Frame([(None, True, False),
                   (False, None, None),
                   (None, None, None)])
    RES = DT[:, [rowfirst(f[:]), rowlast(f[:])]]
    assert_equals(RES, dt.Frame([[True, False, None], [False, False, None]]))


@pytest.mark.parametrize("st", stypes_int)
def test_rowfirstlast_ints(st):
    DT = dt.Frame([(7, 5, 19, 22),
                   (None, 1, 2, None),
                   (None, None, None, None)], stype=st)
    RES = DT[:, [rowfirst(f[:]), rowlast(f[:])]]
    assert_equals(RES, dt.Frame([[7, 1, None], [22, 2, None]], stype=st))


@pytest.mark.parametrize("st", stypes_float)
def test_rowfirstlast_floats(st):
    DT = dt.Frame([(3.0, 7.0, math.nan),
                   (math.inf, None, None),
                   (math.nan, 2.5, -111)], stype=st)
    RES = DT[:, [rowfirst(f[:]), rowlast(f[:])]]
    assert_equals(RES, dt.Frame([[3.0, math.inf, 2.5],
                                 [7.0, math.inf, -111.0]], stype=st))


@pytest.mark.parametrize("st", stypes_str)
def test_rowfirstlast_strs(st):
    DT = dt.Frame([("a", None, "b", None),
                   (None, None, "x", None),
                   ("", "", "AHA!", "last")], stype=st)
    RES = DT[:, [rowfirst(f[:]), rowlast(f[:])]]
    assert_equals(RES, dt.Frame([["a", "x", ""],
                                 ["b", "x", "last"]], stype=st))


def test_rowfirstlast_incompatible():
    DT = dt.Frame(A=["a", "b", "c"], B=[1, 3, 4])
    with pytest.raises(TypeError, match="Incompatible column types"):
        assert DT[:, rowfirst(f[:])]




#-------------------------------------------------------------------------------
# rowmax(), rowmin()
#-------------------------------------------------------------------------------

def test_rowminmax_simple():
    DT = dt.Frame([[3], [-6], [17], [0], [5.4]])
    RES = DT[:, [rowmax(f[:]), rowmin(f[:])]]
    assert_equals(RES, dt.Frame([[17.0], [-6.0]]))


def test_rowminmax_int8():
    DT = dt.Frame([[4], [None], [1], [3]], stype=dt.int8)
    RES = DT[:, [rowmax(f[:]), rowmin(f[:])]]
    assert_equals(RES, dt.Frame([[4], [1]], stype=dt.int32))


def test_rowminmax_nas():
    DT = dt.Frame([[None]] * 3, stype=dt.int64)
    RES = DT[:, [rowmax(f[:]), rowmin(f[:])]]
    assert_equals(RES, dt.Frame([[None], [None]], stype=dt.int64))


def test_rowminmax_almost_nas():
    DT = dt.Frame([[None], [None], [1], [None]], stype=dt.float64)
    RES = DT[:, [rowmax(f[:]), rowmin(f[:])]]
    assert_equals(RES, dt.Frame([[1.0], [1.0]]))


def test_rowminmax_floats():
    import sys
    maxflt = sys.float_info.max
    DT = dt.Frame([(7.5, math.nan, 4.1),
                   (math.nan, math.inf, None),
                   (math.inf, -math.inf, None),
                   (maxflt, math.inf, -maxflt)])
    RES = DT[:, [rowmax(f[:]), rowmin(f[:])]]
    assert_equals(RES, dt.Frame([[7.5, math.inf, +math.inf, math.inf],
                                 [4.1, math.inf, -math.inf, -maxflt]]))




#-------------------------------------------------------------------------------
# rowmean()
#-------------------------------------------------------------------------------

def test_rowmean_simple():
    DT = dt.Frame(A=range(5))
    assert_equals(rowmean(DT), dt.Frame(range(5), stype=dt.float64))


def test_rowmean_floats():
    DT = dt.Frame([(1.5, 6.4, 0.0, None, 7.22),
                   (2.0, -1.1, math.inf, 4.0, 3.2),
                   (1.5, 9.9, None, None, math.nan),
                   (math.inf, -math.inf, None, 0.0, math.nan)])
    RES = DT[:, rowmean(f[:])]
    x = (1.5 + 6.4 + 7.22) / 4
    assert_equals(RES, dt.Frame([x, math.inf, 5.7, None]))


def test_rowmean_wrong_types():
    DT = dt.Frame(A=[3, 5, 6], B=["a", "d", "e"])
    with pytest.raises(TypeError, match="Function `rowmean` expects a sequence "
                                        "of numeric columns"):
        assert rowmean(DT)




#-------------------------------------------------------------------------------
# rowsd()
#-------------------------------------------------------------------------------

def test_rowsd_single_column():
    DT = dt.Frame(A=range(5))
    assert_equals(rowsd(DT), dt.Frame([math.nan]*5))


def test_rowsd_same_columns():
    DT = dt.Frame([range(5)] * 10)
    assert_equals(rowsd(DT), dt.Frame([0.0]*5))


def test_rowsd_floats():
    DT = dt.Frame([(1.5, 6.4, 0.0, None, 7.22),
                   (2.0, -1.1, math.inf, 4.0, 3.2),
                   (1.5, 9.9, None, None, math.nan),
                   (math.inf, -math.inf, None, 0.0, math.nan)])
    RES = DT[:, rowsd(f[:])]
    std1 = 3.5676696409094086
    std3 = 5.939696961966999
    assert_equals(RES, dt.Frame([std1, None, std3, None]))


def test_rowmean_wrong_types():
    DT = dt.Frame(A=[3, 5, 6], B=["a", "d", "e"])
    with pytest.raises(TypeError, match="Function `rowsd` expects a sequence "
                                        "of numeric columns"):
        assert rowsd(DT)




#-------------------------------------------------------------------------------
# rowsum()
#-------------------------------------------------------------------------------

def test_rowsum_bools():
    DT = dt.Frame([[True, True, False, False, None,  None],
                   [True, False, True, False, True,  None],
                   [True, True,  True, False, False, None]])
    RES = DT[:, rowsum(f[:])]
    assert_equals(RES, dt.Frame([3, 2, 2, 0, 1, 0], stype=dt.int32))



def test_rowsum_int8():
    DT = dt.Frame([[3, 7, -1, 0, None],
                   [15, 19, 1, None, 1],
                   [0, 111, 88, 3, 4]], stype=dt.int8)
    RES = DT[:, rowsum(f[int])]
    assert_equals(RES, dt.Frame([18, 137, 88, 3, 5], stype=dt.int32))


def test_rowsum_different_types():
    DT = dt.Frame([[3, 4, 6, 9, None, 1],
                   [True, False, False, None, True, True],
                   [14, 15, -1, 2, 7, -11],
                   [4, 10, 3, None, 0, -8]],
                  stypes=[dt.int8, dt.bool8, dt.int64, dt.int32])
    RES = DT[:, rowsum(f[:])]
    assert_equals(RES, dt.Frame([22, 29, 8, 11, 8, -17], stype=dt.int64))


def test_rowsum_promote_to_float32():
    DT = dt.Frame([[2], [7], [11]], stypes=[dt.int32, dt.float32, dt.int64])
    assert_equals(rowsum(DT),
                  dt.Frame([20], stype=dt.float32))


def test_rowsum_promote_to_float64():
    DT = dt.Frame([[2], [3], [1], [5], [None]],
                  stypes=[dt.int8, dt.float64, dt.int64, dt.float32, dt.int16])
    assert_equals(rowsum(DT),
                  dt.Frame([11], stype=dt.float64))




