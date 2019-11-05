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
#
# Test assigning Frames and Frame-like expressions (i.e. a numpy array or a
# pandas DataFrame).
#
#-------------------------------------------------------------------------------
import math
import pytest
from datatable import f, dt
from tests import assert_equals



#-------------------------------------------------------------------------------
# Assign a Frame
#-------------------------------------------------------------------------------

def test_assign_frame_as_new_column():
    DT = dt.Frame(A=range(5))
    DT["B"] = dt.Frame(list("abcde"))
    assert_equals(DT, dt.Frame(A=range(5), B=list("abcde")))


def test_assign_frame_into_existing_column():
    DT = dt.Frame(A=range(5))
    DT["A"] = dt.Frame(list("abcde"))
    assert_equals(DT, dt.Frame(A=list("abcde")))


def test_assign_frame_change_stype():
    DT = dt.Frame(A=range(5), stype=dt.int32)
    DT["A"] = dt.Frame([0]*5, stype=dt.int16)
    assert_equals(DT, dt.Frame(A=[0]*5, stype=dt.int16))


def test_assign_frame_slice():
    DT0 = dt.Frame(A=range(10))
    DT1 = dt.Frame([i / 2 for i in range(100)])
    DT0[:, "A"] = DT1[:10, :]
    assert_equals(DT0, dt.Frame(A=[i / 2 for i in range(10)]))


def test_assign_multi_column_frame():
    DT = dt.Frame(A=range(5))
    DT[:, ["B", "C"]] = dt.Frame([["foo"]*5, list("abcde")])
    assert_equals(DT, dt.Frame(A=range(5), B=["foo"]*5, C=list("abcde")))


def test_assign_single_row_frame():
    DT = dt.Frame(A=[0, 1, 2], B=[3, 4, 5])
    DT[:, "C"] = dt.Frame([7])
    assert_equals(DT, dt.Frame(A=[0, 1, 2], B=[3, 4, 5], C=[7, 7, 7]))


def test_assign_single_row_frame2():
    DT = dt.Frame(A=[0, 1, 2], B=[3, 4, 5])
    DT[:, ("C", "D")] = dt.Frame([[7], [8]])
    assert_equals(DT, dt.Frame(A=[0, 1, 2], B=[3, 4, 5], C=[7]*3, D=[8]*3))


def test_assign_single_column_frame():
    DT = dt.Frame(A=[0, 1, 2])
    DT[:, ("C", "Z", "E")] = dt.Frame([9, 8, 7])
    assert_equals(DT, dt.Frame(A=[0, 1, 2], C=[9, 8, 7], Z=[9, 8, 7],
                               E=[9, 8, 7]))


def test_assign_wrong_nrows():
    DT = dt.Frame(A=[3, 4, 5], B=['a', 't', 'k'])
    with pytest.raises(ValueError):
        DT[:, "D"] = dt.Frame(range(5))
    assert DT.shape == (3, 2)
    with pytest.raises(ValueError):
        DT[:, "D"] = dt.Frame()  # 0 rows


def test_assign_wrong_ncols():
    DT = dt.Frame(A=range(5))
    with pytest.raises(ValueError):
        DT["B"] = dt.Frame([[3]*5, [4]*5])
    with pytest.raises(ValueError):
        DT[:, ["B", "C", "D"]] = dt.Frame([[3]*5, [4]*5])


def test_assign_0col_frame():
    DT = dt.Frame(A=range(5))
    R = DT[:, []]
    assert R.shape == (5, 0)
    with pytest.raises(ValueError):
        DT["Z"] = R



#-------------------------------------------------------------------------------
# Assign Frame to a subframe
#-------------------------------------------------------------------------------

def test_assign_to_new_column_rows_subset():
    DT = dt.Frame(A=range(5))
    DT[::2, "D"] = dt.Frame([6, 7, 8])
    assert_equals(DT, dt.Frame(A=range(5), D=[6, None, 7, None, 8]))


def test_assign_frame_to_single_cell():
    DT = dt.Frame(A=range(5), B=range(5))
    DT[0, "B"] = dt.Frame([17])
    assert_equals(DT, dt.Frame(A=range(5), B=[17, 1, 2, 3, 4]))


def test_assign_frame_to_filtered_rows():
    DT = dt.Frame(A=range(5))
    DT[f.A < 3, "A"] = dt.Frame([4, 5, 7])
    assert_equals(DT, dt.Frame(A=[4, 5, 7, 3, 4]))


def test_assign_frame_to_subframe_with_upcast():
    DT = dt.Frame(A=range(5), stype=dt.int8)
    DT[:2, "A"] = dt.Frame([3, 4], stype=dt.int64)
    assert_equals(DT, dt.Frame(A=[3, 4, 2, 3, 4], stype=dt.int64))


def test_assign_frame_to_subframe_with_downcast():
    DT = dt.Frame(A=range(5), stype=dt.int32)
    DT[:2, "A"] = dt.Frame([13, 14], stype=dt.int8)
    assert_equals(DT, dt.Frame(A=[13, 14, 2, 3, 4], stype=dt.int32))


@pytest.mark.parametrize("st1, st2",
    [(dt.int16, dt.int8), (dt.int32, dt.int8), (dt.int32, dt.int16),
     (dt.int64, dt.int8), (dt.int64, dt.int16), (dt.int64, dt.int32),
     (dt.float32, dt.int8), (dt.float32, dt.int16), (dt.float32, dt.int32),
     (dt.float32, dt.int64), (dt.float64, dt.int8), (dt.float64, dt.int16),
     (dt.float64, dt.int32), (dt.float64, dt.int64), (dt.float64, dt.float32)])
def test_assign_downcasts(st1, st2):
    DT = dt.Frame(A=range(5), stype=st1)
    assert DT.stype == st1
    DT[:2, 0] = dt.Frame([-1, None], stype=st2)
    assert_equals(DT, dt.Frame(A=[-1, None, 2, 3, 4], stype=st1))


def test_assign_frame_to_subframe_wrong_ltype():
    DT = dt.Frame(A=range(5))
    with pytest.raises(TypeError, match="Cannot assign float value to column "
                                        "`A` of type int32"):
        DT[:2, "A"] = dt.Frame([0.5, -0.5])
    with pytest.raises(TypeError):
        DT[:2, "A"] = dt.Frame(["nope"])
    with pytest.raises(TypeError):
        DT[:2, "A"] = dt.Frame([True, False])


def test_assign_frame_to_subframe_wrong_nrows():
    DT = dt.Frame(A=range(5))
    with pytest.raises(ValueError):
        DT[:2, "A"] = dt.Frame([6, 5, 4, 3, 2])




#-------------------------------------------------------------------------------
# Assign a numpy array
#-------------------------------------------------------------------------------

def test_assign_nparray(numpy):
    DT = dt.Frame(A=range(5))
    DT[:, "B"] = numpy.array([7, 6, 5, 4, 3])
    assert_equals(DT, dt.Frame(A=range(5), B=[7, 6, 5, 4, 3],
                               stypes={"B": dt.int64}))


def test_assign_nparray_str(numpy):
    DT = dt.Frame(A=range(3))
    DT[:, "S"] = numpy.array(["foo", "bar", "meh..."])
    assert_equals(DT, dt.Frame(A=range(3), S=["foo", "bar", "meh..."]))


def test_assign_masked_array(numpy):
    arr = numpy.array([3, 6, 12, 4, 0])
    arr = numpy.ma.array(arr, mask=[False, False, True, True, False])
    DT = dt.Frame(A=range(5))
    DT["M"] = arr
    assert_equals(DT, dt.Frame(A=range(5), M=[3, 6, None, None, 0],
                               stypes={"M": dt.int64}))


def test_assign_nparray_slice(numpy):
    DT = dt.Frame(A=range(5))
    DT["S"] = numpy.array(range(100))[3:12:2]
    assert_equals(DT, dt.Frame(A=range(5), S=[3, 5, 7, 9, 11],
                               stypes={"S": dt.int64}))


def test_assign_nparray_overwrite_column(numpy):
    DT = dt.Frame(A=range(5))
    DT["A"] = numpy.array([0.0, 0.5, 1.0, 1.5, 2.0])
    assert_equals(DT, dt.Frame(A=[0.0, 0.5, 1.0, 1.5, 2.0]))


def test_assign_nparray_overwrite_column2(numpy):
    DT = dt.Frame(A=range(5))
    DT["A"] = numpy.array(range(5), dtype="int8")
    assert_equals(DT, dt.Frame(A=range(5), stype=dt.int8))


def test_assign_nparray_wrong_shape(numpy):
    DT = dt.Frame(A=range(5))
    with pytest.raises(ValueError):
        DT[:, "B"] = numpy.array([3, 14, 15])
    with pytest.raises(ValueError):
        DT[:, "C"] = numpy.array([[1, 2, 3], [4, 5, 6]])


@pytest.mark.xfail()
def test_assign_nparray_reducible_shape(numpy):
    arr = numpy.array([1, 2, 3], dtype=numpy.int32)
    DT = dt.Frame(A=[0, 5, 10])
    DT["B"] = numpy.reshape(arr, (3, 1, 1))
    DT["C"] = numpy.reshape(arr, (1, 3, 1))
    DT["D"] = numpy.reshape(arr, (1, 1, 3))
    assert_equals(DT, dt.Frame(A=[0, 5, 10], B=[1, 2, 3], C=[1, 2, 3],
                               D=[1, 2, 3]))


def test_assign_2d_nparray(numpy):
    arr = numpy.array([[1, 2], [3, 4], [5, 6]], dtype=numpy.int32)
    DT = dt.Frame(A=range(3))
    DT[:, ["B", "C"]] = arr
    assert_equals(DT, dt.Frame(A=[0, 1, 2], B=[1, 3, 5], C=[2, 4, 6]))




#-------------------------------------------------------------------------------
# Assign pandas DataFrame
#-------------------------------------------------------------------------------

def test_assign_pdframe(pandas):
    DT = dt.Frame(A=range(5))
    DT[:, "B"] = pandas.DataFrame({"Z": [3, 4, 2, 1, 0]}, dtype="int32")
    assert_equals(DT, dt.Frame(A=range(5), B=[3, 4, 2, 1, 0]))


def test_assign_pdframe_wrong_shape(pandas):
    DT = dt.Frame(A=range(5))
    with pytest.raises(ValueError):
        DT["B"] = pandas.DataFrame(list(range(10)))
    with pytest.raises(ValueError):
        DT[:, ["B", "C", "D"]] = pandas.DataFrame([[1]*5] * 2)


def test_assign_pdseries(pandas):
    DT = dt.Frame(A=range(5))
    DT[:, "B"] = pandas.Series([2.3, 3.4, 5.6, 7.8, 0])
    assert_equals(DT, dt.Frame(A=range(5), B=[2.3, 3.4, 5.6, 7.8, 0.0]))
