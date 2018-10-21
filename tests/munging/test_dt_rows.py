#!/usr/bin/env python
# © H2O.ai 2018; -*- encoding: utf-8 -*-
#   This Source Code Form is subject to the terms of the Mozilla Public
#   License, v. 2.0. If a copy of the MPL was not distributed with this
#   file, You can obtain one at http://mozilla.org/MPL/2.0/.
#-------------------------------------------------------------------------------
import pytest
import datatable as dt
from datatable import stype, ltype, f
from tests import same_iterables, has_llvm


#-------------------------------------------------------------------------------
# Prepare fixtures & helper functions
#-------------------------------------------------------------------------------

@pytest.fixture(name="dt0")
def _dt0():
    nan = float("nan")
    return dt.Frame([
        [0,   1,   1,  None,    0,  0,    1, None,   1,    1],  # bool
        [7, -11,   9, 10000, None,  0,    0,   -1,   1, None],  # int
        [5,   1, 1.3,   0.1,  1e5,  0, -2.6,  -14, nan,    2],  # real
    ], names=["colA", "colB", "colC"])


def assert_valueerror(df, rows, error_message):
    with pytest.raises(ValueError) as e:
        df(rows=rows)
    assert str(e.type) == "<class 'datatable.ValueError'>"
    assert error_message in str(e.value)


def assert_typeerror(df, rows, error_message):
    with pytest.raises(TypeError) as e:
        df(rows=rows)
    assert str(e.type) == "<class 'datatable.TypeError'>"
    assert error_message in str(e.value)


def as_list(df):
    return df.topython()


def is_slice(df):
    return df.internal.isview and df.internal.rowindex_type == "slice"


def is_arr(df):
    return df.internal.isview and df.internal.rowindex_type == "arr32"



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
    assert dt0.stypes == (stype.bool8, stype.int16, stype.float64)
    assert str(dt0.internal.__class__) == "<class 'datatable.core.DataTable'>"
    assert dt0.internal.isview is False
    assert dt0.internal.rowindex_type is None
    dt0.internal.check()


def test_rows_ellipsis(dt0):
    """Both dt(...) and dt() should select all rows and all columns."""
    dt1 = dt0()
    dt1.internal.check()
    assert dt1.shape == (10, 3)
    assert not dt1.internal.isview
    dt1 = dt0(...)
    dt1.internal.check()
    assert dt1.shape == (10, 3)
    assert not dt1.internal.isview



#-------------------------------------------------------------------------------
# Integer selector tests
#
# Test row selectors which are simple integers, e.g.:
#     dt(5)
# should select the 6-th row of the datatable. Negative integers are also
# allowed, and should count from the end of the datatable.
#-------------------------------------------------------------------------------

@pytest.mark.parametrize("i", range(-10, 10))
def test_rows_integer1(dt0, i):
    dt1 = dt0(i)
    dt1.internal.check()
    assert dt1.shape == (1, 3)
    assert dt1.names == dt0.names
    assert dt1.ltypes == dt0.ltypes
    assert is_slice(dt1)
    assert dt1.topython() == [[col[i]] for col in dt0.topython()]


def test_rows_integer2(dt0):
    assert dt0(0).topython() == [[0], [7], [5]]
    assert dt0(-2).topython()[:2] == [[1], [1]]
    assert dt0(-2).topython()[2][0] is None  # nan turns into None
    assert dt0(2).topython() == [[1], [9], [1.3]]
    assert dt0(4).topython() == [[0], [None], [100000]]
    assert dt0[1, :].topython() == [[1], [-11], [1]]


def test_rows_integer3(dt0):
    assert_valueerror(dt0, 10,
                      "Row `10` is invalid for datatable with 10 rows")
    assert_valueerror(dt0, -11,
                      "Row `-11` is invalid for datatable with 10 rows")
    assert_valueerror(dt0, -20,
                      "Row `-20` is invalid for datatable with 10 rows")
    assert_valueerror(dt0, 10**19,
                      "Row `10000000000000000000` is invalid for datatable "
                      "with 10 rows")


def test_rows_integer_empty_dt():
    df = dt.Frame()
    assert_valueerror(df, 0, "Row `0` is invalid for datatable with 0 rows")
    assert_valueerror(df, -1, "Row `-1` is invalid for datatable with 0 rows")



#-------------------------------------------------------------------------------
# Slice selector tests
#
# Test row selectors which are slices, such as:
#     dt[:5, :]
#     dt[3:-2, :]
#     dt[::-1, :]
#     dt(rows=slice(None, None, -1))
#-------------------------------------------------------------------------------

@pytest.mark.parametrize("sliceobj, nrows", [(slice(None, -2), 8),
                                             (slice(0, 7), 7),
                                             (slice(5, 9), 4),
                                             (slice(9, 5), 0),
                                             (slice(None, None, 100), 1),
                                             (slice(None, None, -1), 10)])
def test_rows_slice1(dt0, sliceobj, nrows):
    dt1 = dt0(sliceobj)
    dt1.internal.check()
    assert dt1.names == dt0.names
    assert dt1.ltypes == dt0.ltypes
    assert nrows == 0 or is_slice(dt1)
    assert dt1.topython() == [col[sliceobj] for col in dt0.topython()]


def test_rows_slice2(dt0):
    assert dt0[:5, :].topython()[0] == [0, 1, 1, None, 0]
    assert dt0[::-1, :].topython()[1] == \
        [None, 1, -1, 0, 0, None, 1e4, 9, -11, 7]
    assert dt0[::3, :].topython()[1] == [7, 10000, 0, None]
    assert dt0[:3:2, :].topython()[1] == [7, 9]
    assert dt0[4:-2, :].topython()[1] == [None, 0, 0, -1]
    assert dt0[20:, :].topython()[2] == []


def test_rows_slice3(dt0):
    assert_valueerror(dt0, slice(0, 10, 0), "step must not be 0")
    # noinspection PyTypeChecker
    assert_valueerror(dt0, slice(3, 5.7),
                      "slice(3, 5.7, None) is not integer-valued")
    # noinspection PyTypeChecker
    assert_valueerror(dt0, slice("colA", "colC"),
                      "slice('colA', 'colC', None) is not integer-valued")



#-------------------------------------------------------------------------------
# Range selector
#
# Test that a range object can be used as a row selector, for example:
#     dt(range(5))
#     dt(range(3, -1, -1))
#-------------------------------------------------------------------------------

@pytest.mark.parametrize("rangeobj", [range(5),
                                      range(2, 3),
                                      range(-5, -2),
                                      range(7, -1),
                                      range(9, -1, -1)])
def test_rows_range1(dt0, rangeobj):
    dt1 = dt0(rangeobj)
    dt1.internal.check()
    assert dt1.shape == (len(rangeobj), 3)
    assert dt1.names == dt0.names
    assert dt1.ltypes == dt0.ltypes
    assert len(rangeobj) == 0 or is_slice(dt1)
    assert dt1.topython() == [[col[i] for i in rangeobj]
                              for col in dt0.topython()]


def test_rows_range2(dt0):
    assert_valueerror(dt0, range(15),
                      "Invalid range(0, 15) for a datatable with 10 rows")
    assert_valueerror(dt0, range(-5, 5),
                      "Invalid range(-5, 5) for a datatable with 10 rows")



#-------------------------------------------------------------------------------
# Generator selector
#
# Test the use of a generator object as a row selector:
#     dt(i for i in range(dt.nrows) if i % 3 <= 1)
#-------------------------------------------------------------------------------

def test_rows_generator(dt0):
    g = (i * 2 for i in range(4))
    assert type(g).__name__ == "generator"
    dt1 = dt0(g)
    dt1.internal.check()
    assert dt1.shape == (4, 3)
    assert is_arr(dt1)
    assert_valueerror(dt0, (i if i % 3 < 2 else str(-i) for i in range(10)),
                      "Invalid row selector '-2' generated at position 2")



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
                          ({1, -1, 0}, 3),
                          ((-1, -1, -1, -1), 4),
                          ([slice(5, None), slice(None, 5)], 10),
                          ([0, 2, range(4), -1], 7),
                          ([4, 9, 3, slice(7), range(10)], 20)])
def test_rows_multislice(dt0, selector, nrows):
    dt1 = dt0(selector)
    dt1.internal.check()
    assert dt1.shape == (nrows, 3)
    assert dt1.names == ("colA", "colB", "colC")
    assert dt1.ltypes == (ltype.bool, ltype.int, ltype.real)
    assert is_arr(dt1)


def test_rows_multislice2(dt0):
    assert as_list(dt0([3, 9, 1, 0]))[0] == [None, 1, 1, 0]
    assert as_list(dt0((2, 5, 5, -1)))[1] == [9, 0, 0, None]
    assert (as_list(dt0([slice(5, None), slice(None, 5)]))[1] ==
            [0, 0, -1, 1, None, 7, -11, 9, 10000, None])
    assert (as_list(dt0([3, 1, slice(-3), 9, 9, 9]))[2] ==
            [0.1, 1, 5, 1, 1.3, 0.1, 100000, 0, -2.6, 2, 2, 2])


def test_rows_multislice3(dt0):
    assert_valueerror(
        dt0, [1, "hey"],
        "Invalid row selector 'hey' at element 1 of the `rows` list")
    assert_valueerror(dt0, [1, -1, 5, -11], "Row `-11` is invalid")



#-------------------------------------------------------------------------------
# Boolean column selector
#
# Test the use of a boolean column as a filter:
#     dt(rows=dt2[0])
#
# The boolean column can be either a 1-column Frame, or a 1-dimensional
# numpy array.
#-------------------------------------------------------------------------------

def test_rows_bool_column(dt0):
    col = dt.Frame([1, 0, 1, 1, None, 0, None, 1, 1, 0])
    dt1 = dt0(col)
    dt1.internal.check()
    assert dt1.shape == (5, 3)
    assert dt1.names == ("colA", "colB", "colC")
    assert dt1.ltypes == (ltype.bool, ltype.int, ltype.real)
    assert is_arr(dt1)
    assert as_list(dt1)[1] == [7, 9, 10000, -1, 1]


def test_rows_bool_column_error(dt0):
    assert_valueerror(dt0, dt.Frame(list(i % 2 for i in range(20))),
                      "`rows` datatable has 20 rows, but applied to a "
                      "datatable with 10 rows")


def test_rows_bool_numpy_array(dt0, numpy):
    arr = numpy.array([True, False, True, True, False,
                       False, True, False, False, True])
    dt1 = dt0(arr)
    dt1.internal.check()
    assert dt1.shape == (5, 3)
    assert dt1.names == ("colA", "colB", "colC")
    assert dt1.ltypes == (ltype.bool, ltype.int, ltype.real)
    assert as_list(dt1)[1] == [7, 9, 10000, 0, None]


def test_rows_bool_numpy_array_error(dt0, numpy):
    assert_valueerror(dt0, numpy.array([True, False, False]),
                      "Cannot apply a boolean numpy array of length 3 "
                      "to a datatable with 10 rows")


def test_rows_bad_column(dt0):
    assert_valueerror(dt0, dt0,
                      "`rows` argument should be a single-column datatable")
    assert_typeerror(dt0, dt.Frame([0.3, 1, 1.5]),
                     "`rows` datatable should be either a boolean or an "
                     "integer column")



#-------------------------------------------------------------------------------
# Integer column selector
#
# Test the use of an integer column as a filter:
#     dt(rows=dt.Frame([0, 5, 10]))
#
# which should produce the same result as
#     dt(rows=[0, 5, 10])
#-------------------------------------------------------------------------------

def test_rows_int_column(dt0):
    col = dt.Frame([0, 3, 0, 1])
    dt1 = dt0(rows=col)
    dt1.internal.check()
    assert dt1.shape == (4, 3)
    assert dt1.names == dt0.names
    assert dt1.ltypes == dt0.ltypes
    assert dt1.topython() == [[0, None, 0, 1],
                              [7, 10000, 7, -11],
                              [5, 0.1, 5, 1]]


def test_rows_int_numpy_array(dt0, numpy):
    arr = numpy.array([7, 1, 0, 3])
    dt1 = dt0(rows=arr)
    dt1.internal.check()
    assert dt1.shape == (4, 3)
    assert dt1.ltypes == dt0.ltypes
    assert dt1.topython() == [[None, 1, 0, None],
                              [-1, -11, 7, 10000],
                              [-14, 1, 5, 0.1]]
    arr2 = numpy.array([[7, 1, 0, 3]])
    dt2 = dt0(rows=arr2)
    dt2.internal.check()
    assert dt1.shape == dt2.shape
    assert dt1.topython() == dt2.topython()
    dt3 = dt0(rows=numpy.array([[7], [1], [0], [3]]))
    dt3.internal.check()
    assert dt3.shape == dt2.shape
    assert dt3.topython() == dt2.topython()


def test_rows_int_numpy_array_errors(dt0, numpy):
    assert_valueerror(dt0, numpy.array([[1, 2], [2, 1], [3, 3]]),
                      "Only a single-dimensional numpy.array is allowed as a "
                      "`rows` argument")
    assert_valueerror(dt0, numpy.array([[[4, 0, 1]]]),
                      "Only a single-dimensional numpy.array is allowed as a "
                      "`rows` argument")
    with pytest.raises(ValueError) as e:
        dt0(rows=numpy.array([5, 11, 3]))
    assert ("The data column contains index 11 which is not allowed for a "
            "Frame with 10 rows" in str(e.value))


def test_rows_int_column_negative(dt0):
    col = dt.Frame([3, 7, -1, 4])
    with pytest.raises(ValueError) as e:
        dt0(rows=col)
    assert "Row indices in integer column cannot be negative" in str(e.value)


def test_rows_int_column_nas(dt0):
    col = dt.Frame([3, None, 2, 4])
    with pytest.raises(ValueError) as e:
        dt0(rows=col)
    assert "RowIndex source column contains NA values" in str(e.value)



#-------------------------------------------------------------------------------
# Expression-based selectors
#
# Test that it is possible to use a lambda-expression as a filter:
#     dt(lambda f: f.colA < f.colB)
#-------------------------------------------------------------------------------

def test_rows_function1(dt0):
    dt1 = dt0(lambda g: g.colA)
    dt2 = dt0(f.colA)
    dt1.internal.check()
    dt2.internal.check()
    assert dt1.shape == dt2.shape == (5, 3)
    assert is_arr(dt1)
    assert as_list(dt1)[1] == [-11, 9, 0, 1, None]
    assert dt1.topython() == dt2.topython()


def test_rows_function2(dt0):
    dt1 = dt0(lambda g: [5, 7, 9])
    dt1.internal.check()
    assert dt1.shape == (3, 3)
    assert as_list(dt1)[1] == [0, -1, None]


def test_rows_function3(dt0):
    dt1 = dt0(lambda g: g.colA < g.colB)
    dt2 = dt0[f.colA < f.colB, :]
    dt1.internal.check()
    dt2.internal.check()
    assert dt1.shape == dt2.shape == (2, 3)
    assert dt1.topython() == dt2.topython() == [[0, 1], [7, 9], [5, 1.3]]


def test_rows_function4(dt0):
    dt1 = dt0(lambda g: g.colB == 0)
    dt2 = dt0(f.colB == 0)
    dt1.internal.check()
    dt2.internal.check()
    assert dt1.shape == dt2.shape == (2, 3)
    assert dt1.topython() == dt2.topython() == [[0, 1], [0, 0], [0, -2.6]]


def test_rows_function_invalid(dt0):
    assert_typeerror(dt0, lambda g: "boooo!",
                     "Unexpected result produced by the `rows` function")


def test_0rows_frame():
    dt0 = dt.Frame(A=[], B=[], stype=int)
    assert dt0.shape == (0, 2)
    dt1 = dt0[f.A == 0, :]
    dt1.internal.check()
    assert dt1.shape == (0, 2)
    assert same_iterables(dt1.names, ("A", "B"))
    dt2 = dt0[:, f.A - f.B]
    dt2.internal.check()
    assert dt2.shape == (0, 1)
    assert dt2.ltypes == (ltype.int, )



#-------------------------------------------------------------------------------
# Test eager selectors
#-------------------------------------------------------------------------------

@pytest.fixture(name="df1")
def _fixture2():
    df1 = dt.Frame([[0, 1, 2, 3, 4, 5, 6, None, 7,    None, 9],
                    [3, 2, 1, 3, 4, 0, 2, None, None, 8,    9.0]],
                   names=["A", "B"])
    df1.internal.check()
    assert df1.ltypes == (ltype.int, ltype.real)
    assert df1.names == ("A", "B")
    return df1


def test_rows_equal(df1):
    dt1 = df1(f.A == f.B, engine="eager")
    dt1.internal.check()
    assert dt1.names == df1.names
    assert dt1.topython() == [[3, 4, None, 9], [3, 4, None, 9]]
    if has_llvm():
        dt2 = df1(f.A == f.B, engine="llvm")
        dt2.internal.check()
        assert dt1.names == dt2.names
        assert dt1.topython() == dt2.topython()


def test_rows_not_equal(df1):
    dt1 = df1(f.A != f.B, engine="eager")
    dt1.internal.check()
    assert dt1.names == df1.names
    assert dt1.topython() == [[0, 1, 2, 5, 6, 7, None],
                              [3, 2, 1, 0, 2, None, 8]]
    if has_llvm():
        dt2 = df1(f.A != f.B, engine="llvm")
        dt2.internal.check()
        assert dt1.names == dt2.names
        assert dt1.topython() == dt2.topython()


def test_rows_less_than(df1):
    dt1 = df1(f.A < f.B, engine="eager")
    dt1.internal.check()
    assert dt1.names == df1.names
    assert dt1.topython() == [[0, 1], [3, 2]]
    if has_llvm():
        dt2 = df1(f.A < f.B, engine="llvm")
        dt2.internal.check()
        assert dt1.names == dt2.names
        assert dt1.topython() == dt2.topython()


def test_rows_greater_than(df1):
    dt1 = df1(f.A > f.B, engine="eager")
    dt1.internal.check()
    assert dt1.names == df1.names
    assert dt1.topython() == [[2, 5, 6], [1, 0, 2]]
    if has_llvm():
        dt2 = df1(f.A > f.B, engine="llvm")
        dt2.internal.check()
        assert dt1.names == dt2.names
        assert dt1.topython() == dt2.topython()


def test_rows_less_than_or_equal(df1):
    dt1 = df1(f.A <= f.B, engine="eager")
    dt1.internal.check()
    assert dt1.names == df1.names
    assert dt1.topython() == [[0, 1, 3, 4, None, 9], [3, 2, 3, 4, None, 9]]
    if has_llvm():
        dt2 = df1(f.A <= f.B, engine="llvm")
        dt2.internal.check()
        assert dt1.names == dt2.names
        assert dt1.topython() == dt2.topython()


def test_rows_greater_than_or_equal(df1):
    dt1 = df1(f.A >= f.B, engine="eager")
    dt1.internal.check()
    assert dt1.names == df1.names
    assert dt1.topython() == [[2, 3, 4, 5, 6, None, 9],
                              [1, 3, 4, 0, 2, None, 9]]
    if has_llvm():
        dt2 = df1(f.A >= f.B, engine="llvm")
        dt2.internal.check()
        assert dt1.names == dt2.names
        assert dt1.topython() == dt2.topython()


def test_rows_compare_to_scalar_gt(df1):
    dt1 = df1(f.A > 3, engine="eager")
    dt1.internal.check()
    assert dt1.names == df1.names
    assert dt1.topython() == [[4, 5, 6, 7, 9], [4, 0, 2, None, 9]]
    if has_llvm():
        dt2 = df1(f.A > 3, engine="llvm")
        dt2.internal.check()
        assert dt1.names == dt2.names
        assert dt1.topython() == dt2.topython()


def test_rows_compare_to_scalar_lt(df1):
    dt1 = df1(f.A < 3, engine="eager")
    dt1.internal.check()
    assert dt1.names == df1.names
    assert dt1.topython() == [[0, 1, 2], [3, 2, 1]]
    if has_llvm():
        dt2 = df1(f.A < 3, engine="llvm")
        dt2.internal.check()
        assert dt1.names == dt2.names
        assert dt1.topython() == dt2.topython()


# noinspection PyComparisonWithNone
def test_rows_compare_to_scalar_eq(df1):
    dt1 = df1(f.A == None, engine="eager")
    dt1.internal.check()
    assert dt1.names == df1.names
    assert dt1.topython() == [[None, None], [None, 8]]
    if has_llvm():
        dt2 = df1(f.A == None, engine="llvm")
        dt2.internal.check()
        assert dt1.names == dt2.names
        assert dt1.topython() == dt2.topython()


def test_rows_unary_minus(df1):
    dt1 = df1(-f.A < -3, engine="eager")
    dt1.internal.check()
    assert dt1.names == df1.names
    assert dt1.topython() == [[4, 5, 6, 7, 9], [4, 0, 2, None, 9]]
    if has_llvm():
        dt2 = df1(-f.A < -3, engine="llvm")
        dt2.internal.check()
        assert dt1.names == dt2.names
        assert dt1.topython() == dt2.topython()


def test_rows_isna(df1):
    from datatable import isna
    dt1 = df1(isna(f.A), engine="eager")
    dt1.internal.check()
    assert dt1.names == df1.names
    assert dt1.topython() == [[None, None], [None, 8]]
    if has_llvm():
        dt2 = df1(isna(f.A), engine="llvm")
        dt2.internal.check()
        assert dt1.names == dt2.names
        assert dt1.topython() == dt2.topython()


def test_rows_mean():
    from datatable import mean
    df0 = dt.Frame(range(10), names=["A"])
    df1 = df0(f.A > mean(f.A), engine="eager")
    df1.internal.check()
    assert df1.topython() == [[5, 6, 7, 8, 9]]


def test_rows_min_max():
    from datatable import min, max
    df0 = dt.Frame(range(10), names=["A"])
    # min = 0, max = 9
    df1 = df0(f.A > (min(f.A) + max(f.A)) / 2, engine="eager")
    df1.internal.check()
    assert df1.topython() == [[5, 6, 7, 8, 9]]


def test_rows_stdev():
    from datatable import sd
    df0 = dt.Frame(range(10), names=["A"])
    # stdev = 3.0276
    df1 = df0(f.A > sd(f.A), engine="eager")
    df1.internal.check()
    assert df1.topython() == [[4, 5, 6, 7, 8, 9]]


def test_rows_strequal():
    df0 = dt.Frame([["a", "bcd", "foo", None, "bee", "xia", "good"],
                    ["a", "bcD", "fooo", None, None, "xia", "evil"]],
                   names=["A", "B"])
    df1 = df0(f.A == f.B, engine="eager")
    df2 = df0(f.A != f.B, engine="eager")
    df3 = df0(f.A == "foo", engine="eager")
    df4 = df0("bcD" == f.B, engine="eager")
    df1.internal.check()
    df2.internal.check()
    assert df1.topython() == [["a", None, "xia"]] * 2
    assert df2.topython() == [["bcd", "foo", "bee", "good"],
                              ["bcD", "fooo", None, "evil"]]
    assert df3.topython() == [["foo"], ["fooo"]]
    assert df4.topython() == [["bcd"], ["bcD"]]




#-------------------------------------------------------------------------------
# Selectors applied to view DataTables
#-------------------------------------------------------------------------------

def test_filter_on_view1():
    df0 = dt.Frame({"A": range(50)})
    df1 = df0[::2, :]
    assert df1.shape == (25, 1)
    df2 = df1(f.A < 10)
    df2.internal.check()
    assert df2.internal.isview
    assert df2.topython() == [[0, 2, 4, 6, 8]]


def test_filter_on_view2():
    df0 = dt.Frame({"A": range(50)})
    df1 = df0[[5, 7, 9, 3, 1, 4, 12, 8, 11, -3], :]
    df2 = df1(rows=lambda g: g.A < 10)
    df2.internal.check()
    assert df2.internal.isview
    assert df2.topython() == [[5, 7, 9, 3, 1, 4, 8]]


def test_filter_on_view3():
    df0 = dt.Frame({"A": range(20)})
    df1 = df0[::5, :]
    df2 = df1(f.A <= 10, engine="eager")
    df2.internal.check()
    assert df2.topython() == [[0, 5, 10]]
    if has_llvm():
        df3 = df1(f.A <= 10, engine="llvm")
        df3.internal.check()
        assert df3.topython() == [[0, 5, 10]]


def test_chained_slice(dt0):
    dt1 = dt0[::2, :]
    dt1.internal.check()
    assert dt1.shape == (5, 3)
    assert dt1.internal.rowindex_type == "slice"
    dt2 = dt1[::-1, :]
    dt2.internal.check()
    assert dt2.shape == (5, 3)
    assert dt2.internal.rowindex_type == "slice"
    assert as_list(dt2)[1] == [1, 0, None, 9, 7]
    dt3 = dt1[2:4, :]
    dt3.internal.check()
    assert dt3.shape == (2, 3)
    assert dt3.internal.rowindex_type == "slice"
    assert as_list(dt3) == [[0, 1], [None, 0], [100000, -2.6]]
    dt4 = dt1[(1, 0, 3, 2), :]
    dt4.internal.check()
    assert dt4.shape == (4, 3)
    assert dt4.internal.rowindex_type == "arr32"
    assert as_list(dt4) == [[1, 0, 1, 0], [9, 7, 0, None], [1.3, 5, -2.6, 1e5]]


def test_chained_array(dt0):
    dt1 = dt0[(2, 5, 1, 1, 1, 0), :]
    dt1.internal.check()
    assert dt1.shape == (6, 3)
    assert dt1.internal.rowindex_type == "arr32"
    dt2 = dt1[::2, :]
    dt2.internal.check()
    assert dt2.shape == (3, 3)
    assert dt2.internal.rowindex_type == "arr32"
    assert as_list(dt2) == [[1, 1, 1], [9, -11, -11], [1.3, 1, 1]]
    dt3 = dt1[::-1, :]
    dt3.internal.check()
    assert dt3.shape == (6, 3)
    assert dt3.internal.rowindex_type == "arr32"
    assert as_list(dt3)[:2] == [[0, 1, 1, 1, 0, 1], [7, -11, -11, -11, 0, 9]]
    dt4 = dt1[(2, 3, 0), :]
    dt4.internal.check()
    assert dt4.shape == (3, 3)
    assert as_list(dt4) == [[1, 1, 1], [-11, -11, 9], [1, 1, 1.3]]



#-------------------------------------------------------------------------------
# Others
#-------------------------------------------------------------------------------

def test_rows_bad_arguments(dt0):
    """
    Test row selectors that are invalid (i.e. not of the types listed above).
    """
    assert_typeerror(dt0, 0.5, "Unexpected `rows` argument: 0.5")
    assert_typeerror(dt0, "5", "Unexpected `rows` argument: '5'")
    assert_typeerror(dt0, True,
                     "Boolean value cannot be used as a `rows` selector")
    assert_typeerror(dt0, False,
                     "Boolean value cannot be used as a `rows` selector")


def test_issue689(tempdir):
    n = 300000  # Must be > 65536
    data = [i % 8 for i in range(n)]
    d0 = dt.Frame(data, names=["A"])
    dt.save(d0, tempdir)
    del d0
    d1 = dt.open(tempdir)
    # Do not check d1! we want it to be lazy at this point
    d2 = d1(rows=lambda g: g[0] == 1)
    d2.internal.check()
    assert d2.shape == (n / 8, 1)
