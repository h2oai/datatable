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
#
# Assign to various [i,j] targets
#
#-------------------------------------------------------------------------------
import datatable as dt
import math
import pytest
from datatable import f, DatatableWarning
from datatable.internal import frame_integrity_check
from tests import assert_equals



#-------------------------------------------------------------------------------
# Assign to small (degenerate) frames
#-------------------------------------------------------------------------------

def test_assign_to_empty_frame_0x0():
    DT = dt.Frame()
    DT[:, "A"] = 'foo!'
    DT["B"] = 0xF00D
    assert_equals(DT, dt.Frame(A=[], B=[], stypes=[dt.str32, dt.int32]))


def test_assign_to_empty_frame_3x0():
    DT = dt.Frame()
    DT.nrows = 3
    DT[:, "A"] = 'foo!'
    assert_equals(DT, dt.Frame(A=['foo!']*3))


def test_assign_to_empty_frame_0x3():
    DT = dt.Frame(A=[], B=[], C=[])
    DT[:, "A":"C"] = False
    with pytest.raises(TypeError):
        DT[:, "A":"C"] = 3
    DT[:, "A":"C"] = True
    assert_equals(DT, dt.Frame(A=[], B=[], C=[], stype=bool))




#-------------------------------------------------------------------------------
# Assign to all rows DT[:, j] = R
#-------------------------------------------------------------------------------

def test_assign_to_one_column():
    DT = dt.Frame(A=range(5), B=[4, 3, 9, 11, -1])
    DT[:, "B"] = 100
    assert_equals(DT, dt.Frame(A=range(5), B=[100]*5))


def test_assign_to_one_column2():
    DT = dt.Frame([range(3)] * 5, names=list("ABCDE"))
    DT[:, 2] = 3
    DT[:, -1] = -1
    assert_equals(DT, dt.Frame(A=range(3), B=range(3), C=[3]*3,
                               D=range(3), E=[-1]*3))


def test_assign_to_new_column():
    DT = dt.Frame(A=range(7))
    DT["Z"] = 2.5
    assert_equals(DT, dt.Frame(A=range(7), Z=[2.5]*7))


def test_assign_to_all():
    DT = dt.Frame([range(5), [4, 3, 9, 11, -1]], names=("A", "B"))
    DT[:, :] = 12
    assert_equals(DT, dt.Frame(A=[12]*5, B=[12]*5))


def test_assign_to_list_of_columns():
    DT = dt.Frame(A=[1, 3, 4], B=[2.5, 17.3, None], C=[4.99, -12, 2.22])
    DT[:, ["A", "C"]] = 35
    assert_equals(DT, dt.Frame(A=[35]*3, B=[2.5, 17.3, None], C=[35.0]*3))




#-------------------------------------------------------------------------------
# Assign to subset of rows
#-------------------------------------------------------------------------------

def test_assign_to_row_slice_single_column():
    DT = dt.Frame(A=range(8))
    DT[::2, "A"] = 100
    assert_equals(DT, dt.Frame(A=[100, 1, 100, 3, 100, 5, 100, 7]))




def test_assign_to_column_slice():
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
    with pytest.raises(TypeError, match="Cannot assign boolean value to "
                                        "column `B` of type float64"):
        f0[:, ["B", "A"]] = False
    f0[:, ["B", "A"]] = 0
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
    frame_integrity_check(f0)
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


def test_assign_to_view2():
    f0 = dt.Frame(A=range(10))
    f0[::2, "B"] = 17
    assert f0.to_list()[1] == [17, None] * 5


def test_assign_to_view3():
    f0 = dt.Frame(A=range(10))
    f0[::2, "B"] = dt.Frame([5, 7, 9, 2, 1])
    assert f0.to_list()[1] == [5, None, 7, None, 9, None, 2, None, 1, None]


def test_assign_to_view4():
    f0 = dt.Frame(A=range(10))
    f0[1::2, "B"] = dt.Frame([3, 4])[[0, 1, 0, 1, 0], :]
    assert f0.to_list()[1] == [None, 3, None, 4, None, 3, None, 4, None, 3]


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
    frame_integrity_check(X)
    X[:, []] = dt.Frame()
    frame_integrity_check(X)
    assert_equals(X, dt.Frame(A=range(10)))


def test_assign_none_all():
    DT = dt.Frame([[True, False], [1, 5], [999, -12], [34.2, math.inf],
                   [0.001, 1e-100], ['foo', 'bar'], ['buzz', '?']],
                  names=list("ABCDEFG"),
                  stypes=['bool', 'int8', 'int64', 'float32', 'float64',
                          'str32', 'str64'])
    DT[:, :] = None
    frame_integrity_check(DT)
    assert DT.shape == (2, 7)
    assert DT.names == tuple("ABCDEFG")
    assert DT.stypes == (dt.bool8, dt.int8, dt.int64, dt.float32, dt.float64,
                         dt.str32, dt.str64)
    assert DT.to_list() == [[None, None]] * 7


def test_assign_none_single():
    DT = dt.Frame(A=range(5))
    DT[:, f.A] = None
    frame_integrity_check(DT)
    assert DT.stypes == (dt.int32,)
    assert DT.to_list() == [[None] * 5]


def test_assign_none_new():
    DT = dt.Frame(A=range(5))
    DT[:, "B"] = None
    frame_integrity_check(DT)
    assert DT.names == ("A", "B")
    assert DT.stypes == (dt.int32, dt.bool8)
    assert DT.to_list() == [[0, 1, 2, 3, 4], [None] * 5]


def test_assign_list_of_exprs():
    DT = dt.Frame(A=range(5))
    DT[:, ["B", "C"]] = [f.A + 1, f.A * 2]
    frame_integrity_check(DT)
    assert DT.names == ("A", "B", "C")
    assert DT.stypes == (dt.int32,) * 3
    assert DT.to_list() == [[0, 1, 2, 3, 4], [1, 2, 3, 4, 5], [0, 2, 4, 6, 8]]


def test_assign_list_duplicates():
    DT = dt.Frame(A=range(5))
    with pytest.warns(DatatableWarning) as ws:
        DT[:, ["B", "B"]] = [f.A + 1, f.A + 2]
    frame_integrity_check(DT)
    assert DT.names == ("A", "B", "B.0")
    assert DT.to_list() == [[0, 1, 2, 3, 4], [1, 2, 3, 4, 5], [2, 3, 4, 5, 6]]
    assert len(ws) == 1
    assert ("Duplicate column name found, and was assigned a unique name: "
            "'B' -> 'B.0'" in ws[0].message.args[0])


def test_stats_after_assign():
    DT = dt.Frame(A=[0, 1, None, 4, 7])
    assert DT.min1() == 0
    assert DT.max1() == 7
    assert DT.countna1() == 1
    DT[:, "A"] = 100
    assert DT.min1() == DT.max1() == 100
    assert DT.countna1() == 0


def test_assign_to_single_column_selector():
    DT = dt.Frame(A=range(5), B=list('abcde'))[:, ['A', 'B']]
    DT["A"] = 17
    assert_equals(DT, dt.Frame([[17] * 5, list('abcde')],
                               names=('A', 'B'),
                               stypes=(dt.int32, dt.str32)))
    DT["C"] = False
    assert_equals(DT, dt.Frame([[17] * 5, list('abcde'), [False] * 5],
                               names=('A', 'B', 'C'),
                               stypes=(dt.int32, dt.str32, dt.bool8)))
    DT[-2] = "hi!"
    assert_equals(DT, dt.Frame([[17] * 5, ["hi!"] * 5, [False] * 5],
                               names=('A', 'B', 'C'),
                               stypes=(dt.int32, dt.str32, dt.bool8)))


def test_assign_nonexisting_column():
    # See #1983: if column `B` is created at a wrong moment in the evaluation
    # sequence, this may seg.fault
    DT = dt.Frame(A=range(5))
    with pytest.raises(ValueError) as e:
        DT[:, "B"] = f.B + 1
    assert ("Column `B` does not exist in the Frame" in str(e.value))
    frame_integrity_check(DT)


@pytest.mark.xfail
def test_assign_wrong_type():
    # Check that if one assignment was correct, the exception during the
    # next assignment rolls the frame back to a consistent state
    DT = dt.Frame(B=range(5))
    with pytest.raises(TypeError):
        DT[:, ["A", "B"]] = 3.3
    frame_integrity_check(DT)


def test_assign_exprs_with_i():
    DT = dt.Frame(A=range(10))
    DT[f.A < 5, f.A] = f.A + 10
    assert_equals(DT, dt.Frame(A=[10, 11, 12, 13, 14, 5, 6, 7, 8, 9]))
