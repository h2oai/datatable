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
import pytest
import datatable as dt
import random
from datatable import stype, ltype, f, by
from datatable.internal import frame_columns_virtual, frame_integrity_check
from tests import same_iterables, noop, assert_equals, isview


#-------------------------------------------------------------------------------
# Prepare fixtures & helper functions
#-------------------------------------------------------------------------------

@pytest.fixture(name="dt0")
def _dt0():
    from math import nan
    T, F = True, False
    return dt.Frame([
        [F,   T,   T,  None,    F,  F,    T, None,   T,    T],  # bool
        [7, -11,   9, 10000, None,  0,    0,   -1,   1, None],  # int
        [5,   1, 1.3,   0.1,  1e5,  0, -2.6,  -14, nan,    2],  # real
    ], names=["colA", "colB", "colC"])


def assert_valueerror(df, rows, error_message):
    with pytest.raises(ValueError) as e:
        noop(df[rows, :])
    assert str(e.type) == "<class 'datatable.ValueError'>"
    assert error_message in str(e.value)


def assert_typeerror(df, rows, error_message):
    with pytest.raises(TypeError) as e:
        noop(df[rows, :])
    assert str(e.type) == "<class 'datatable.TypeError'>"
    assert error_message in str(e.value)


def as_list(df):
    return df.to_list()





#-------------------------------------------------------------------------------
# Basic tests
#-------------------------------------------------------------------------------

def test_dt0_properties(dt0):
    """Test basic properties of the Frame object."""
    assert isinstance(dt0, dt.Frame)
    assert dt0.nrows == 10
    assert dt0.ncols == 3
    assert dt0.shape == (10, 3)  # must be a tuple, not a list!
    assert dt0.names == ("colA", "colB", "colC")
    assert dt0.ltypes == (ltype.bool, ltype.int, ltype.real)
    assert dt0.stypes == (stype.bool8, stype.int32, stype.float64)
    assert not any(frame_columns_virtual(dt0))
    frame_integrity_check(dt0)


def test_rows_ellipsis(dt0):
    """Both dt(...) and dt() should select all rows and all columns."""
    dt1 = dt0[None, :]
    frame_integrity_check(dt1)
    assert dt1.shape == (10, 3)
    assert not isview(dt1)
    dt1 = dt0[..., :]
    frame_integrity_check(dt1)
    assert dt1.shape == (10, 3)
    assert not isview(dt1)



#-------------------------------------------------------------------------------
# Integer selector tests
#
# Test row selectors which are simple integers, e.g.:
#     dt[5, :]
# should select the 6-th row of the Frame. Negative integers are also
# allowed, and should count from the end of the Frame.
#-------------------------------------------------------------------------------

@pytest.mark.parametrize("i", range(-10, 10))
def test_rows_integer1(dt0, i):
    dt1 = dt0[i, :]
    frame_integrity_check(dt1)
    assert dt1.shape == (1, 3)
    assert dt1.names == dt0.names
    assert dt1.ltypes == dt0.ltypes
    assert isview(dt1)
    assert dt1.to_list() == [[col[i]] for col in dt0.to_list()]


def test_rows_integer2(dt0):
    assert dt0[0, :].to_list() == [[0], [7], [5]]
    assert dt0[-2, :].to_list()[:2] == [[1], [1]]
    assert dt0[-2, :].to_list()[2][0] is None  # nan turns into None
    assert dt0[2, :].to_list() == [[1], [9], [1.3]]
    assert dt0[4, :].to_list() == [[0], [None], [100000]]
    assert dt0[1, :].to_list() == [[1], [-11], [1]]


def test_rows_integer3(dt0):
    assert_valueerror(dt0, 10,
                      "Row `10` is invalid for a frame with 10 rows")
    assert_valueerror(dt0, -11,
                      "Row `-11` is invalid for a frame with 10 rows")
    assert_valueerror(dt0, -20,
                      "Row `-20` is invalid for a frame with 10 rows")
    assert_valueerror(dt0, 10**18,
                      "Row `1000000000000000000` is invalid for a frame "
                      "with 10 rows")


def test_rows_integer_empty_dt():
    df = dt.Frame()
    assert_valueerror(df, 0, "Row `0` is invalid for a frame with 0 rows")
    assert_valueerror(df, -1, "Row `-1` is invalid for a frame with 0 rows")



#-------------------------------------------------------------------------------
# Slice selector tests
#
# Test row selectors which are slices, such as:
#     dt[:5, :]
#     dt[3:-2, :]
#     dt[::-1, :]
#-------------------------------------------------------------------------------

@pytest.mark.parametrize("sliceobj, nrows", [(slice(None, -2), 8),
                                             (slice(0, 7), 7),
                                             (slice(5, 9), 4),
                                             (slice(9, 5), 0),
                                             (slice(None, None, 100), 1),
                                             (slice(None, None, -1), 10)])
def test_rows_slice1(dt0, sliceobj, nrows):
    dt1 = dt0[sliceobj, :]
    frame_integrity_check(dt1)
    assert dt1.names == dt0.names
    assert dt1.ltypes == dt0.ltypes
    assert nrows == 0 or isview(dt1)
    assert dt1.to_list() == [col[sliceobj] for col in dt0.to_list()]


def test_rows_0step_slice():
    DT = dt.Frame(range(5))
    res = DT[3:100:0, :]
    frame_integrity_check(res)
    assert res.to_list() == [[3] * 100]


def test_rows_slice2(dt0):
    assert dt0[:5, :].to_list()[0] == [0, 1, 1, None, 0]
    assert dt0[::-1, :].to_list()[1] == \
        [None, 1, -1, 0, 0, None, 1e4, 9, -11, 7]
    assert dt0[::3, :].to_list()[1] == [7, 10000, 0, None]
    assert dt0[:3:2, :].to_list()[1] == [7, 9]
    assert dt0[4:-2, :].to_list()[1] == [None, 0, 0, -1]
    assert dt0[20:, :].to_list()[2] == []


def test_rows_slice3(dt0):
    assert dt0[2:10:0, 0].to_list()[0] == [1] * 10
    assert dt0[-3:7:0, 2].to_list()[0] == [-14.0] * 7


def test_rows_slice_errors0(dt0):
    # noinspection PyTypeChecker
    assert_typeerror(dt0, slice(3, 5.7),
                     "slice(3, 5.7, None) is neither integer- nor "
                     "string- valued")


def test_rows_slice_errors1(dt0):
    # noinspection PyTypeChecker
    assert_typeerror(dt0, slice("colA", "colC"),
                     "A string slice cannot be used as a row selector")


def test_slice_errors2(dt0):
    assert_valueerror(dt0, slice(None, 2, 0),
                      "When a slice's step is 0, the first and the second "
                      "parameters may not be missing")
    assert_valueerror(dt0, slice(-1, None, 0),
                      "When a slice's step is 0, the first and the second "
                      "parameters may not be missing")
    assert_valueerror(dt0, slice(0, 0, 0),
                      "When a slice's step is 0, the second parameter (count) "
                      "must be positive")
    assert_valueerror(dt0, slice(1, -2, 0),
                      "When a slice's step is 0, the second parameter (count) "
                      "must be positive")


def test_slice_after_resize():
    """Issue #1592"""
    DT = dt.Frame(A=['cat'])
    DT.nrows = 3
    RES = DT[2:, :]
    frame_integrity_check(RES)
    assert RES.to_list() == [[None]]




#-------------------------------------------------------------------------------
# Range selector
#
# Test that a range object can be used as a row selector, for example:
#     dt[range(5), :]
#     dt[range(3, -1, -1), :]
#-------------------------------------------------------------------------------

@pytest.mark.parametrize("rangeobj", [range(5),
                                      range(2, 3),
                                      range(1, 1),
                                      range(-5, -2),
                                      range(7, -1),
                                      range(9, -1, -1)])
def test_rows_range1(dt0, rangeobj):
    dt1 = dt0[rangeobj, :]
    frame_integrity_check(dt1)
    assert dt1.shape == (len(rangeobj), 3)
    assert dt1.names == dt0.names
    assert dt1.ltypes == dt0.ltypes
    assert len(rangeobj) == 0 or isview(dt1)
    assert dt1.to_list() == [[col[i] for i in rangeobj]
                             for col in dt0.to_list()]


def test_rows_range2(dt0):
    assert_valueerror(
        dt.Frame(range(1)), range(5),
        "range(5) cannot be applied to a Frame with 1 row")
    assert_valueerror(
        dt0, range(15),
        "range(15) cannot be applied to a Frame with 10 rows")
    assert_valueerror(
        dt0, range(-5, 5),
        "range(-5, 5) cannot be applied to a Frame with 10 rows")



#-------------------------------------------------------------------------------
# Generator selector
#
# Test the use of a generator object as a row selector:
#     dt(i for i in range(dt.nrows) if i % 3 <= 1)
#-------------------------------------------------------------------------------

def test_rows_generator(dt0):
    g = (i * 2 for i in range(4))
    assert type(g).__name__ == "generator"
    dt1 = dt0[g, :]
    frame_integrity_check(dt1)
    assert dt1.shape == (4, 3)
    assert isview(dt1)


def test_rows_generator_bad(dt0):
    assert_typeerror(
        dt0, (i if i % 3 < 2 else str(-i) for i in range(10)),
        "Invalid item of type string at index 2 in the i-selector list")




#-------------------------------------------------------------------------------
# "Multi-slice" selectors
#
# Test that it is possible to combine multiple row-selectors in an array:
#     dt([0, 5, 2, 1])
#     dt([slice(None, None, 2), slice(1, None, 2)])
#     dt([0, 1] * 100)
#-------------------------------------------------------------------------------

@pytest.mark.parametrize("selector, nrows",
                         [([2, 7, 0, 9], 4),
                          ([1, -1, 0], 3),
                          ((-1, -1, -1, -1), 4),
                          ([slice(5, None), slice(None, 5)], 10),
                          ([0, 2, range(4), -1], 7),
                          ([4, 9, 3, slice(7), range(10)], 20)])
def test_rows_multislice(dt0, selector, nrows):
    dt1 = dt0[selector, :]
    frame_integrity_check(dt1)
    assert dt1.shape == (nrows, 3)
    assert dt1.names == ("colA", "colB", "colC")
    assert dt1.ltypes == (ltype.bool, ltype.int, ltype.real)
    assert isview(dt1)


def test_rows_multislice2(dt0):
    assert as_list(dt0[[3, 9, 1, 0], :])[0] == [None, 1, 1, 0]
    assert as_list(dt0[(2, 5, 5, -1), :])[1] == [9, 0, 0, None]
    assert (as_list(dt0[[slice(5, None), slice(None, 5)], :])[1] ==
            [0, 0, -1, 1, None, 7, -11, 9, 10000, None])
    assert (as_list(dt0[[3, 1, slice(-3), 9, 9, 9], :])[2] ==
            [0.1, 1, 5, 1, 1.3, 0.1, 100000, 0, -2.6, 2, 2, 2])


def test_rows_multislice4():
    DT = dt.Frame(range(20))
    res = DT[[range(5), 3, -1, range(8, -2, -2)], :]
    frame_integrity_check(res)
    assert res.ncols == 1
    assert res.to_list()[0] == [0, 1, 2, 3, 4, 3, 19, 8, 6, 4, 2, 0]


def test_rows_multislice5():
    DT = dt.Frame(range(20))
    res = DT[[range(3), slice(4, 105, 0)], :]
    frame_integrity_check(res)
    assert res.ncols == 1
    assert res.to_list()[0] == [0, 1, 2] + [4] * 105


def test_rows_multislice6():
    DT = dt.Frame(range(20))
    res = DT[[slice(100), slice(4, None, -2)], :]
    frame_integrity_check(res)
    assert res.ncols == 1
    assert res.to_list()[0] == list(range(20)) + [4, 2, 0]


def test_rows_multislice7():
    DT = dt.Frame(range(20))
    res = DT[[range(-5, 0, 2)], :]
    frame_integrity_check(res)
    assert res.to_list() == [[15, 17, 19]]


def test_rows_multislice_invalid1(dt0):
    assert_typeerror(
        dt0, [1, "hey"],
        "Invalid item of type string at index 1 in the i-selector list")


def test_rows_multislice_invalid2(dt0):
    assert_valueerror(
        dt0, [1, -1, 5, -11],
        "Index -11 is invalid for a Frame with 10 rows")


def test_rows_multislice_invalid3(dt0):
    assert_valueerror(
        dt0, [0, range(4, -4, -1)],
        "range(4, -4, -1) cannot be applied to a Frame with 10 rows")


def test_rows_multislice_invalid4(dt0):
    assert_typeerror(
        dt0, [slice("A", "Z")],
        "Invalid expression of type string-slice at index 0 in the "
        "i-selector list")


def test_rows_multislice_invalid5(dt0):
    assert_valueerror(
        dt0, [slice(3, -1, 0)], "When a slice's step is 0, the second "
                                "parameter (count) must be positive")
    assert_valueerror(
        dt0, [slice(3, None, 0)], "When a slice's step is 0, the first and "
                                  "the second parameters may not be missing")
    assert_valueerror(
        dt0, [slice(None, 6, 0)], "When a slice's step is 0, the first and "
                                  "the second parameters may not be missing")




#-------------------------------------------------------------------------------
# Boolean column selector
#
# Test the use of a boolean column as a filter:
#     dt[dt2[0], :]
#
# The boolean column can be either a 1-column Frame, or a 1-dimensional
# numpy array.
#-------------------------------------------------------------------------------

def test_rows_bool_column(dt0):
    col = dt.Frame([1, 0, 1, 1, None, 0, None, 1, 1, 0], stype=bool)
    dt1 = dt0[col, :]
    frame_integrity_check(dt1)
    assert dt1.shape == (5, 3)
    assert dt1.names == ("colA", "colB", "colC")
    assert dt1.ltypes == (ltype.bool, ltype.int, ltype.real)
    assert isview(dt1)
    assert as_list(dt1)[1] == [7, 9, 10000, -1, 1]


def test_rows_bool_column_error(dt0):
    assert_valueerror(
        dt0, dt.Frame(list(bool(i % 2) for i in range(20))),
        "`i` selector has 20 rows, but applied to a Frame with 10 rows")


def test_rows_bad_column(dt0):
    assert_valueerror(
        dt0, dt0,
        "Only a single-column Frame may be used as `i` selector, instead got "
        "a Frame with 3 columns")
    assert_typeerror(
        dt0, dt.Frame([0.3, 1, 1.5]),
        "A Frame which is used as an `i` selector should be either boolean or "
        "integer, instead got `float64`")



#-------------------------------------------------------------------------------
# Integer column selector
#
# Test the use of an integer column as a filter:
#     dt[dt.Frame([0, 5, 10]), :]
#
# which should produce the same result as
#     dt[[0, 5, 10], :]
#-------------------------------------------------------------------------------

def test_rows_int_column(dt0):
    col = dt.Frame([0, 3, 0, 1])
    dt1 = dt0[col, :]
    frame_integrity_check(dt1)
    assert dt1.shape == (4, 3)
    assert dt1.names == dt0.names
    assert dt1.ltypes == dt0.ltypes
    assert dt1.to_list() == [[0, None, 0, 1],
                             [7, 10000, 7, -11],
                             [5, 0.1, 5, 1]]


def test_rows_int_column2():
    DT = dt.Frame(range(10))
    col = dt.Frame([3, 4, None, 0, None])
    res = DT[col, :]
    frame_integrity_check(res)
    assert res.shape == (5, 1)
    assert res.to_list() == [[3, 4, None, 0, None]]


def test_rows_int_column_negative(dt0):
    col = dt.Frame([3, 7, -3, 4])
    assert_valueerror(
        dt0, col,
        "An integer column used as an `i` selector contains an invalid negative "
        "index: -3")


def test_rows_int_column_large(dt0):
    col = dt.Frame([3, 7, 93, 4])
    assert_valueerror(
        dt0, col,
        "An integer column used as an `i` selector contains index 93 which is "
        "not valid for a Frame with 10 rows")


def test_rows_int_column_0rows(dt0):
    col = dt.Frame([[]], stype="int64")
    assert col.shape == (0, 1)
    assert col.stypes == (stype.int64,)
    res = dt0[col, :]
    frame_integrity_check(res)
    assert res.shape == (0, dt0.ncols)


def test_issue1970():
    DT = dt.Frame(A=[], stype=dt.float32)
    DT.nrows = 2
    RES = DT[dt.Frame([None], stype=int), :]
    frame_integrity_check(RES)
    assert_equals(RES, dt.Frame(A=[None], stype=DT.stype))



#-------------------------------------------------------------------------------
# Numpy-array selectors
#-------------------------------------------------------------------------------

def test_rows_numpy_array(numpy):
    DT = dt.Frame(range(1000))
    idx = numpy.arange(0, 1000, 5)
    res = DT[idx, :]
    frame_integrity_check(res)
    assert res.shape == (200, 1)
    assert res.to_list() == [list(range(0, 1000, 5))]


def test_rows_numpy_array_big(numpy):
    DT = dt.Frame(range(1000))
    idx = numpy.arange(900, 1200, 5)
    assert_valueerror(
        DT, idx,
        "An integer column used as an `i` selector contains index 1195 "
        "which is not valid for a Frame with 1000 rows")


def test_rows_int_numpy_array_shapes(dt0, numpy):
    arr1 = numpy.array([7, 1, 0, 3])
    arr2 = numpy.array([[7, 1, 0, 3]]).T
    arr3 = numpy.array([[7], [1], [0], [3]])
    for arr in [arr1, arr2, arr3]:
        dt1 = dt0[arr, :]
        frame_integrity_check(dt1)
        assert dt1.shape == (4, 3)
        assert dt1.ltypes == dt0.ltypes
        assert dt1.to_list() == [[None, 1, 0, None],
                                 [-1, -11, 7, 10000],
                                 [-14, 1, 5, 0.1]]


def test_rows_int_numpy_array_errors1(dt0, numpy):
    assert_valueerror(
        dt0, numpy.array([[1, 2], [2, 1], [3, 3]]),
        "Only a single-column Frame may be used as `i` selector, "
        "instead got a Frame with 2 columns")


def test_rows_int_numpy_array_errors2(dt0, numpy):
    assert_valueerror(
        dt0, numpy.array([[[4, 0, 1]]]),
        "Cannot create Frame from a 3-D numpy array")


def test_rows_int_numpy_array_errors3(dt0, numpy):
    assert_valueerror(
        dt0, numpy.array([5, 11, 3]),
        "An integer column used as an `i` selector contains index 11 which is "
        "not valid for a Frame with 10 rows")


def test_rows_bool_numpy_array(dt0, numpy):
    arr = numpy.array([True, False, True, True, False,
                       False, True, False, False, True])
    dt1 = dt0[arr, :]
    frame_integrity_check(dt1)
    assert dt1.shape == (5, 3)
    assert dt1.names == ("colA", "colB", "colC")
    assert dt1.ltypes == (ltype.bool, ltype.int, ltype.real)
    assert as_list(dt1)[1] == [7, 9, 10000, 0, None]


def test_rows_bool_numpy_array_error(dt0, numpy):
    assert_valueerror(
        dt0, numpy.array([True, False, False]),
        "A boolean column used as `i` selector has 3 rows, but applied to "
        "a Frame with 10 rows")


def test_rows_bool_numpy_array_error2(dt0, numpy):
    assert_typeerror(
        dt0, numpy.array([1.7, 3.4, 0.5] + [0.0] * 7),
        "A Frame which is used as an `i` selector should be either boolean "
        "or integer, instead got `float64`")



#-------------------------------------------------------------------------------
# Expression-based selectors
#
# Test that it is possible to use a lambda-expression as a filter:
#     dt[f.colA < f.colB, :]
#-------------------------------------------------------------------------------

def test_rows_expr1(dt0):
    dt2 = dt0[f.colA, :]
    frame_integrity_check(dt2)
    assert dt2.shape == (5, 3)
    assert dt2.to_list()[1] == [-11, 9, 0, 1, None]


def test_rows_expr3(dt0):
    dt2 = dt0[f.colA < f.colB, :]
    frame_integrity_check(dt2)
    assert dt2.shape == (2, 3)
    assert dt2.to_list() == [[0, 1], [7, 9], [5, 1.3]]


def test_rows_expr4(dt0):
    dt1 = dt0[f.colB == 0, :]
    frame_integrity_check(dt1)
    assert dt1.shape == (2, 3)
    assert dt1.to_list() == [[0, 1], [0, 0], [0, -2.6]]


def test_0rows_frame():
    dt0 = dt.Frame(A=[], B=[], stype=int)
    assert dt0.shape == (0, 2)
    dt1 = dt0[f.A == 0, :]
    frame_integrity_check(dt1)
    assert dt1.shape == (0, 2)
    assert same_iterables(dt1.names, ("A", "B"))
    dt2 = dt0[:, f.A - f.B]
    frame_integrity_check(dt2)
    assert dt2.shape == (0, 1)
    assert dt2.ltypes == (ltype.int, )


def test_select_wrong_stype():
    dt0 = dt.Frame(A=range(5), B=range(5))
    assert_typeerror(
        dt0, f.A + f.B,
        "Filter expression must be boolean, instead it was of type int32")



#-------------------------------------------------------------------------------
# Test eager selectors
#-------------------------------------------------------------------------------

@pytest.fixture(name="df1")
def _fixture2():
    df1 = dt.Frame([[0, 1, 2, 3, 4, 5, 6, None, 7,    None, 9],
                    [3, 2, 1, 3, 4, 0, 2, None, None, 8,    9.0]],
                   names=["A", "B"])
    frame_integrity_check(df1)
    assert df1.ltypes == (ltype.int, ltype.real)
    assert df1.names == ("A", "B")
    return df1


def test_rows_equal(df1):
    dt1 = df1[f.A == f.B, :]
    frame_integrity_check(dt1)
    assert dt1.names == df1.names
    assert dt1.to_list() == [[3, 4, None, 9], [3, 4, None, 9]]


def test_rows_not_equal(df1):
    dt1 = df1[f.A != f.B, :]
    frame_integrity_check(dt1)
    assert dt1.names == df1.names
    assert dt1.to_list() == [[0, 1, 2, 5, 6, 7, None],
                             [3, 2, 1, 0, 2, None, 8]]


def test_rows_less_than(df1):
    dt1 = df1[f.A < f.B, :]
    frame_integrity_check(dt1)
    assert dt1.names == df1.names
    assert dt1.to_list() == [[0, 1], [3, 2]]


def test_rows_greater_than(df1):
    dt1 = df1[f.A > f.B, :]
    frame_integrity_check(dt1)
    assert dt1.names == df1.names
    assert dt1.to_list() == [[2, 5, 6], [1, 0, 2]]


def test_rows_less_than_or_equal(df1):
    dt1 = df1[f.A <= f.B, :]
    frame_integrity_check(dt1)
    assert dt1.names == df1.names
    assert dt1.to_list() == [[0, 1, 3, 4, None, 9], [3, 2, 3, 4, None, 9]]


def test_rows_greater_than_or_equal(df1):
    dt1 = df1[f.A >= f.B, :]
    frame_integrity_check(dt1)
    assert dt1.names == df1.names
    assert dt1.to_list() == [[2, 3, 4, 5, 6, None, 9],
                             [1, 3, 4, 0, 2, None, 9]]


def test_rows_compare_to_scalar_gt(df1):
    dt1 = df1[f.A > 3, :]
    frame_integrity_check(dt1)
    assert dt1.names == df1.names
    assert dt1.to_list() == [[4, 5, 6, 7, 9], [4, 0, 2, None, 9]]


def test_rows_compare_to_scalar_lt(df1):
    dt1 = df1[f.A < 3, :]
    frame_integrity_check(dt1)
    assert dt1.names == df1.names
    assert dt1.to_list() == [[0, 1, 2], [3, 2, 1]]


# noinspection PyComparisonWithNone
def test_rows_compare_to_scalar_eq(df1):
    dt1 = df1[f.A == None, :]
    frame_integrity_check(dt1)
    assert dt1.names == df1.names
    assert dt1.to_list() == [[None, None], [None, 8]]


def test_rows_unary_minus(df1):
    dt1 = df1[-f.A < -3, :]
    frame_integrity_check(dt1)
    assert dt1.names == df1.names
    assert dt1.to_list() == [[4, 5, 6, 7, 9], [4, 0, 2, None, 9]]


def test_rows_isna(df1):
    from datatable import isna
    dt1 = df1[isna(f.A), :]
    frame_integrity_check(dt1)
    assert dt1.names == df1.names
    assert dt1.to_list() == [[None, None], [None, 8]]


def test_rows_mean():
    from datatable import mean
    df0 = dt.Frame(A=range(10))
    df1 = df0[f.A > mean(f.A), :]
    frame_integrity_check(df1)
    assert df1.to_list() == [[5, 6, 7, 8, 9]]


def test_rows_min_max():
    from datatable import min, max
    df0 = dt.Frame(A=range(10))
    # min = 0, max = 9
    df1 = df0[f.A > (min(f.A) + max(f.A)) / 2, :]
    frame_integrity_check(df1)
    assert df1.to_list() == [[5, 6, 7, 8, 9]]


def test_rows_stdev():
    from datatable import sd
    df0 = dt.Frame(A=range(10))
    # stdev = 3.0276
    df1 = df0[f.A > sd(f.A), :]
    frame_integrity_check(df1)
    assert df1.to_list() == [[4, 5, 6, 7, 8, 9]]


def test_rows_strequal():
    df0 = dt.Frame([["a", "bcd", "foo", None, "bee", "xia", "good"],
                    ["a", "bcD", "fooo", None, None, "xia", "evil"]],
                   names=["A", "B"])
    df1 = df0[f.A == f.B, :]
    df2 = df0[f.A != f.B, :]
    df3 = df0[f.A == "foo", :]
    df4 = df0["bcD" == f.B, :]
    frame_integrity_check(df1)
    frame_integrity_check(df2)
    assert df1.to_list() == [["a", None, "xia"]] * 2
    assert df2.to_list() == [["bcd", "foo", "bee", "good"],
                             ["bcD", "fooo", None, "evil"]]
    assert df3.to_list() == [["foo"], ["fooo"]]
    assert df4.to_list() == [["bcd"], ["bcD"]]




#-------------------------------------------------------------------------------
# Selectors applied to view DataTables
#-------------------------------------------------------------------------------

def test_filter_on_view1():
    df0 = dt.Frame({"A": range(50)})
    df1 = df0[::2, :]
    assert df1.shape == (25, 1)
    df2 = df1[f.A < 10, :]
    frame_integrity_check(df2)
    assert isview(df2)
    assert df2.to_list() == [[0, 2, 4, 6, 8]]


def test_filter_on_view2():
    df0 = dt.Frame(A=range(50))
    df1 = df0[[5, 7, 9, 3, 1, 4, 12, 8, 11, -3], :]
    df2 = df1[f.A < 10, :]
    frame_integrity_check(df2)
    assert isview(df2)
    assert df2.to_list() == [[5, 7, 9, 3, 1, 4, 8]]


def test_filter_on_view3():
    df0 = dt.Frame(A=range(20))
    df1 = df0[::5, :]
    df2 = df1[f.A <= 10, :]
    frame_integrity_check(df2)
    assert df2.to_list() == [[0, 5, 10]]


def test_chained_slice0(dt0):
    dt1 = dt0[::2, :]
    frame_integrity_check(dt1)
    assert dt1.shape == (5, 3)
    assert isview(dt1)


def test_chained_slice1(dt0):
    dt2 = dt0[::2, :][::-1, :]
    frame_integrity_check(dt2)
    assert dt2.shape == (5, 3)
    assert isview(dt2)
    assert dt2.to_list()[1] == [1, 0, None, 9, 7]


def test_chained_slice2(dt0):
    dt3 = dt0[::2, :][2:4, :]
    frame_integrity_check(dt3)
    assert dt3.shape == (2, 3)
    assert isview(dt3)
    assert dt3.to_list() == [[0, 1], [None, 0], [100000, -2.6]]


def test_chained_slice3(dt0):
    dt4 = dt0[::2, :][[1, 0, 3, 2], :]
    frame_integrity_check(dt4)
    assert dt4.shape == (4, 3)
    assert isview(dt4)
    assert dt4.to_list() == [[1, 0, 1, 0], [9, 7, 0, None], [1.3, 5, -2.6, 1e5]]


def test_chained_array0(dt0):
    dt1 = dt0[[2, 5, 1, 1, 1, 0], :]
    frame_integrity_check(dt1)
    assert dt1.shape == (6, 3)
    assert isview(dt1)


def test_chained_array1(dt0):
    dt2 = dt0[[2, 5, 1, 1, 1, 0], :][::2, :]
    frame_integrity_check(dt2)
    assert dt2.shape == (3, 3)
    assert isview(dt2)
    assert as_list(dt2) == [[1, 1, 1], [9, -11, -11], [1.3, 1, 1]]


def test_chained_array2(dt0):
    dt3 = dt0[[2, 5, 1, 1, 1, 0], :][::-1, :]
    frame_integrity_check(dt3)
    assert dt3.shape == (6, 3)
    assert isview(dt3)
    assert as_list(dt3)[:2] == [[0, 1, 1, 1, 0, 1], [7, -11, -11, -11, 0, 9]]


def test_chained_array3(dt0):
    dt4 = dt0[[2, 5, 1, 1, 1, 0], :][(2, 3, 0), :]
    frame_integrity_check(dt4)
    assert dt4.shape == (3, 3)
    assert as_list(dt4) == [[1, 1, 1], [-11, -11, 9], [1, 1, 1.3]]



#-------------------------------------------------------------------------------
# Others
#-------------------------------------------------------------------------------

def test_rows_bad_arguments(dt0):
    """
    Test row selectors that are invalid (i.e. not of the types listed above).
    """
    assert_typeerror(
        dt0, 0.5, "A floating-point value cannot be used as a row selector")
    assert_typeerror(
        dt0, "59", "A string value cannot be used as a row selector")
    assert_typeerror(
        dt0, True, "A boolean value cannot be used as a row selector")
    assert_typeerror(
        dt0, False, "A boolean value cannot be used as a row selector")


def test_issue689(tempfile_jay):
    n = 300000  # Must be > 65536
    data = [i % 8 for i in range(n)]
    d0 = dt.Frame(data, names=["A"])
    d0.to_jay(tempfile_jay)
    del d0
    d1 = dt.fread(tempfile_jay)
    # Do not check d1! we want it to be lazy at this point
    d2 = d1[f[0] == 1, :]
    frame_integrity_check(d2)
    assert d2.shape == (n / 8, 1)


@pytest.mark.parametrize("st", dt.ltype.int.stypes)
def test_issue1437(st):
    d0 = dt.Frame(range(100))
    d1 = d0[range(20), :]
    d2 = d1[dt.Frame(range(10), stype=st), :]
    frame_integrity_check(d2)
    assert d2.to_list() == [list(range(10))]


def test_issue1602():
    # Note: the sequence of operations to cause the bug was very particular;
    # none of these steps were optional.
    DT = dt.Frame(A=range(4))
    DT = DT[:2, :]
    del DT[[0, 1], :]
    DT = DT[::1, :]
    DT.nrows = 1
    assert DT.to_list() == [[None]]


def test_issue2055(numpy):
    DT = dt.cbind(
        dt.Frame(A=[1, 2]),
        dt.Frame(numpy.ma.array([True, True], mask=[False, False]))
    )
    DT.nrows = 1
    DT = DT.copy()
    frame_integrity_check(DT)
    assert DT.to_list() == [[1], [True]]



#-------------------------------------------------------------------------------
# (i=slice) + by
#-------------------------------------------------------------------------------

def test_grouped_slice_simple():
    DT = dt.Frame(A=[1,2,3,1,2,3], B=[3,4,3,4,3,4])
    res1 = DT[1:, :, by("B")]
    res2 = DT[:2, :, by("B")]
    res3 = DT[::-1, :, by("B")]
    assert_equals(res1, dt.Frame(B=[3, 3, 4, 4], A=[3, 2, 1, 3]))
    assert_equals(res2, dt.Frame(B=[3, 3, 4, 4], A=[1, 3, 2, 1]))
    assert_equals(res3, dt.Frame(B=[3, 3, 3, 4, 4, 4], A=[2, 3, 1, 3, 1, 2]))


@pytest.mark.parametrize("seed", [random.getrandbits(32) for _ in range(10)])
def test_grouped_slice_random(seed):
    random.seed(seed)

    # Construct a slice
    a = random.randint(-5, 10)
    if a > 5:
        a = None
    b = random.randint(-10, 30)
    if b > 20:
        b = None
    c = random.randint(-3, 5)
    if c == 0:
        if a is None:
            a = 0
        if b is None:
            b = 5
        if b <= 0:
            b = -b + 1
    if c > 3:
        c = None
    s = slice(a, b, c)

    # Construct the grouped data, and the result
    data_grouped = []
    result = []
    ngroups = random.randint(2, 20)
    j0 = 0
    for i in range(ngroups):
        groupsize = 1 + int(random.expovariate(0.1))
        group = [(i, j + j0) for j in range(groupsize)]
        j0 += groupsize
        data_grouped.append(group)
        if c == 0:
            if -groupsize <= a < groupsize:
                result.extend([group[a]] * b)
        else:
            result.extend(group[s])

    # Shuffle the rows of the grouped data
    source_data = []
    while data_grouped:
        g = random.randint(0, len(data_grouped) - 1)
        elem = data_grouped[g].pop(0)
        source_data.append(elem)
        if len(data_grouped[g]) == 0:
            del data_grouped[g]

    # Compute the result using datatable
    DT = dt.Frame(source_data, names=["G", "X"], stype=dt.int32)
    RES = DT[s, :, by("G")]
    EXP = dt.Frame(result, names=["G", "X"], stype=dt.int32)

    assert_equals(RES, EXP)
