#!/usr/bin/env python
# -*- coding: utf-8 -*-
#-------------------------------------------------------------------------------
# Copyright 2018 H2O.ai
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
import math
from datatable import f
from tests import assert_equals



def test_assign_column_slice():
    src = [[1, 5, 10, 100], [7.3, 14, -2, 1.2e5], ["foo", None, None, "g"]]
    f0 = dt.Frame(src)
    assert f0.ltypes == (dt.ltype.int, dt.ltype.real, dt.ltype.str)
    f0[:, :-1] = -1
    assert f0.shape == (4, 3)
    assert f0.to_list() == [[-1] * 4, [-1.0] * 4, src[2]]
    assert f0.ltypes == (dt.ltype.int, dt.ltype.real, dt.ltype.str)


def test_assign_column_array():
    f0 = dt.Frame({"A": range(10)})
    assert f0.ltypes == (dt.ltype.int,)
    f0[:, "B"] = 3.5
    assert f0.names == ("A", "B")
    assert f0.ltypes == (dt.ltype.int, dt.ltype.real)
    assert f0.shape == (10, 2)
    f0[:, "C"] = "foo"
    assert f0.ltypes == (dt.ltype.int, dt.ltype.real, dt.ltype.str)
    assert f0.to_list() == [list(range(10)), [3.5] * 10, ["foo"] * 10]
    f0[:, ["B", "A"]] = False
    assert f0.ltypes == (dt.ltype.int, dt.ltype.real, dt.ltype.str)
    assert f0.to_list() == [[0] * 10, [0.0] * 10, ["foo"] * 10]
    f0[:, "A"] = None
    assert f0.ltypes == (dt.ltype.int, dt.ltype.real, dt.ltype.str)
    assert f0.to_list() == [[None] * 10, [0.0] * 10, ["foo"] * 10]


def test_assign_single_cell():
    f0 = dt.Frame([[1, 5, 7, 10], [3, 14, -2, 0]])
    for i in range(4):
        for j in range(2):
            f0[i, j] = i + j
    f0.internal.check()
    assert f0.ltypes == (dt.ltype.int, ) * 2
    assert f0.to_list() == [[0, 1, 2, 3], [1, 2, 3, 4]]


def test_assign_filtered():
    f0 = dt.Frame({"A": range(10)})
    f0[f.A < 5, :] = -1
    assert f0.to_list() == [[-1, -1, -1, -1, -1, 5, 6, 7, 8, 9]]
    f0[f.A < 0, :] = None
    assert f0.to_list() == [[None, None, None, None, None, 5, 6, 7, 8, 9]]


def test_assign_to_view():
    f0 = dt.Frame({"A": range(10)})
    f1 = f0[::2, :]
    f1[:, "AA"] = "test"
    assert f1.names == ("A", "AA")
    assert f1.ltypes == (dt.ltype.int, dt.ltype.str)
    assert f1.to_list() == [list(range(0, 10, 2)), ["test"] * 5]
    assert f0.to_list() == [list(range(10))]


def test_assign_frame():
    f0 = dt.Frame({"A": range(10)})
    f1 = dt.Frame([i / 2 for i in range(100)])
    f0[:, "A"] = f1[:10, :]
    assert f0.names == ("A",)
    assert f0.ltypes == (dt.ltype.real,)


def test_assign_string_columns():
    f0 = dt.Frame(A=["One", "two", "three", None, "five"])
    f0[dt.isna(f.A), f.A] = dt.Frame(["FOUR"])
    assert f0.names == ("A", )
    assert f0.stypes == (dt.stype.str32,)
    assert f0.to_list() == [["One", "two", "three", "FOUR", "five"]]


def test_assign_string_columns2():
    f0 = dt.Frame(A=["One", "two", "three", None, "five"])
    f0[[2, 0, 4], "A"] = dt.Frame([None, "Oh my!", "infinity"])
    f0[1, "A"] = dt.Frame([None], stype=dt.str32)
    assert f0.names == ("A", )
    assert f0.stypes == (dt.stype.str32,)
    assert f0.to_list() == [["Oh my!", None, None, None, "infinity"]]


def test_assign_empty_frame():
    # See issue #1544
    X = dt.Frame(A=range(10))
    X[:, []] = X[:, []]
    X.internal.check()
    X[:, []] = dt.Frame()
    X.internal.check()
    assert_equals(X, dt.Frame(A=range(10)))


def test_assign_none_all():
    DT = dt.Frame([[True, False], [1, 5], [999, -12], [34.2, math.inf],
                   [0.001, 1e-100], ['foo', 'bar'], ['buzz', '?']],
                  names=list("ABCDEFG"),
                  stypes=['bool', 'int8', 'int64', 'float32', 'float64',
                          'str32', 'str64'])
    DT[:, :] = None
    DT.internal.check()
    assert DT.shape == (2, 7)
    assert DT.names == tuple("ABCDEFG")
    assert DT.stypes == (dt.bool8, dt.int8, dt.int64, dt.float32, dt.float64,
                         dt.str32, dt.str64)
    assert DT.to_list() == [[None, None]] * 7


def test_assign_none_single():
    DT = dt.Frame(A=range(5))
    DT[:, f.A] = None
    DT.internal.check()
    assert DT.stypes == (dt.int32,)
    assert DT.to_list() == [[None] * 5]


def test_assign_none_new():
    DT = dt.Frame(A=range(5))
    DT[:, "B"] = None
    DT.internal.check()
    assert DT.names == ("A", "B")
    assert DT.stypes == (dt.int32, dt.bool8)
    assert DT.to_list() == [[0, 1, 2, 3, 4], [None] * 5]
