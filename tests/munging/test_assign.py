#!/usr/bin/env python
# © H2O.ai 2018; -*- encoding: utf-8 -*-
#   This Source Code Form is subject to the terms of the Mozilla Public
#   License, v. 2.0. If a copy of the MPL was not distributed with this
#   file, You can obtain one at http://mozilla.org/MPL/2.0/.
#-------------------------------------------------------------------------------
import datatable as dt
from datatable import f
from tests import assert_equals



def test_assign_column_slice():
    src = [[1, 5, 10, 100], [7.3, 14, -2, 1.2e5], ["foo", None, None, "g"]]
    f0 = dt.Frame(src)
    assert f0.ltypes == (dt.ltype.int, dt.ltype.real, dt.ltype.str)
    f0[:, 1:] = -1
    assert f0.shape == (4, 3)
    assert f0.to_list() == [src[0], [-1] * 4, [-1] * 4]
    assert f0.ltypes == (dt.ltype.int, ) * 3


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
    f0[:, ["B", "C"]] = False
    assert f0.ltypes == (dt.ltype.int, dt.ltype.bool, dt.ltype.bool)
    assert f0.to_list() == [list(range(10)), [False] * 10, [False] * 10]
    f0[:, "A"] = None
    assert f0.ltypes == (dt.ltype.bool,) * 3


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
