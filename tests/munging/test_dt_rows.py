#!/usr/bin/env python3
# Copyright 2017 H2O.ai; Apache License Version 2.0;  -*- encoding: utf-8 -*-
import pytest
from tests import datatable as dt
from math import isnan


#-------------------------------------------------------------------------------
# Prepare fixtures & helper functions
#-------------------------------------------------------------------------------

@pytest.fixture()
def dt0():
    nan = float("nan")
    return dt.DataTable({
        #        0    1    2      3     4   5     6     7    8     9
        "colA": [0,   1,   1,  None,    0,  0,    1, None,   1,    1],  # bool
        "colB": [7, -11,   9, 10000, None,  0,    0,   -1,   1, None],  # int
        "colC": [5,   1, 1.3,   0.1,  1e5,  0, -2.6,  -14, nan,    2],  # real
    })


def assert_valueerror(datatable, rows, error_message):
    with pytest.raises(ValueError) as e:
        datatable(rows=rows)
    assert str(e.type) == "<class 'dt.ValueError'>"
    assert error_message in str(e.value)


def assert_typeerror(datatable, rows, error_message):
    with pytest.raises(TypeError) as e:
        datatable(rows=rows)
    assert str(e.type) == "<class 'dt.TypeError'>"
    assert error_message in str(e.value)


def as_list(datatable):
    nrows, ncols = datatable.shape
    return datatable.internal.window(0, nrows, 0, ncols).data


def is_slice(dt, cols=range(3)):
    return dt.internal.isview and dt.internal.rowmapping_type == "slice"


def is_arr(dt, cols=range(3)):
    return dt.internal.isview and dt.internal.rowmapping_type == "arr32"


def is_slice_or_arr(dt, nrows):
    return (nrows == 0 or
            dt.internal.isview and dt.internal.rowmapping_type == "slice")


#-------------------------------------------------------------------------------
# Run the tests
#-------------------------------------------------------------------------------

def test_dt0_properties(dt0):
    """Test basic properties of the DataTable object."""
    assert isinstance(dt0, dt.DataTable)
    assert dt0.nrows == 10
    assert dt0.ncols == 3
    assert dt0.shape == (10, 3)  # must be a tuple, not a list!
    assert dt0.names == ("colA", "colB", "colC")
    assert dt0.types == ("bool", "int", "real")
    assert dt0.stypes == ("i1b", "i2i", "f8r")
    assert str(dt0.internal.__class__) == "<class '_datatable.DataTable'>"
    assert dt0.internal.isview is False
    assert dt0.internal.rowmapping_type is None
    assert dt0.internal.verify_integrity() is None


def test_rows_ellipsis(dt0):
    """Both dt(...) and dt() should select all rows and all columns."""
    dt1 = dt0()
    assert dt1.shape == (10, 3)
    assert is_slice(dt1)
    dt1 = dt0(...)
    assert dt1.shape == (10, 3)
    assert is_slice(dt1)


def test_rows_integer(dt0):
    """
    Test row selectors which are simple integers, such as:
        dt(5)
    should select the 6-th row of the datatable. Negative integers are also
    allowed, and should count from the end of the datatable.
    """
    for i in range(-10, 10):
        dt1 = dt0(i)
        assert dt1.shape == (1, 3)
        assert dt1.names == ("colA", "colB", "colC")
        assert dt1.types == ("bool", "int", "real")
        assert is_slice(dt1)
    assert as_list(dt0(0)) == [[0], [7], [5]]
    assert as_list(dt0(-2))[:2] == [[1], [1]]
    assert isnan(as_list(dt0(-2))[2][0])
    assert as_list(dt0(2)) == [[1], [9], [1.3]]
    assert as_list(dt0(4)) == [[0], [None], [100000]]
    assert as_list(dt0[1, :]) == [[1], [-11], [1]]
    assert_valueerror(dt0, 10, "Row `10` is invalid for datatable with 10 rows")
    assert_valueerror(dt0, -11, "Row `-11` is invalid for datatable with 10")
    assert_valueerror(dt0, -20, "Row `-20` is invalid for datatable with 10")
    assert_valueerror(dt0, 10**19, "Row `10000000000000000000` is invalid")


def test_rows_slice(dt0):
    """
    Test row selectors which are slices, such as:
        dt[:5, :]
        dt[3:-2, :]
        dt[::-1, :]
        dt(rows=slice(None, None, -1))
    """
    for sliceobj, nrows in [(slice(None, -2), 8),
                            (slice(0, 7), 7),
                            (slice(5, 9), 4),
                            (slice(9, 5), 0),
                            (slice(None, None, 100), 1),
                            (slice(None, None, -1), 10)]:
        dt1 = dt0(sliceobj)
        assert dt1.shape == (nrows, 3)
        assert dt1.names == ("colA", "colB", "colC")
        assert dt1.types == ("bool", "int", "real")
        assert is_slice_or_arr(dt1, nrows)
    assert as_list(dt0[:5, :])[0] == [0, 1, 1, None, 0]
    assert as_list(dt0[::-1, :])[1] == [None, 1, -1, 0, 0, None, 1e4, 9, -11, 7]
    assert as_list(dt0[::3, :])[1] == [7, 10000, 0, None]
    assert as_list(dt0[:3:2, :])[1] == [7, 9]
    assert as_list(dt0[4:-2, :])[1] == [None, 0, 0, -1]
    assert as_list(dt0[20:, :])[2] == []
    assert_valueerror(dt0, slice(0, 10, 0), "step must not be 0")
    # noinspection PyTypeChecker
    assert_valueerror(dt0, slice(3, 5.7),
                      "slice(3, 5.7, None) is not integer-valued")
    # noinspection PyTypeChecker
    assert_valueerror(dt0, slice("colA", "colC"),
                      "slice('colA', 'colC', None) is not integer-valued")


def test_rows_range(dt0):
    """
    Test that a range object can be used as a row selector, for example:
        dt(range(5))
        dt(range(3, -1, -1))
    """
    for rangeobj in [range(5),
                     range(2, 3),
                     range(-5, -2),
                     range(7, -1),
                     range(9, -1, -1)]:
        dt1 = dt0(rangeobj)
        nrows = len(rangeobj)
        assert isinstance(rangeobj, range)
        assert dt1.shape == (nrows, 3)
        assert dt1.names == ("colA", "colB", "colC")
        assert dt1.types == ("bool", "int", "real")
        assert is_slice_or_arr(dt1, nrows)
    assert_valueerror(dt0, range(15),
                      "Invalid range(0, 15) for a datatable with 10 rows")
    assert_valueerror(dt0, range(-5, 5),
                      "Invalid range(-5, 5) for a datatable with 10 rows")


def test_rows_generator(dt0):
    """
    Test the use of a generator object as a row selector:
        dt(i for i in range(dt.nrows) if i % 3 <= 1)
    """
    g = (i * 2 for i in range(4))
    assert type(g).__name__ == "generator"
    dt1 = dt0(g)
    assert dt1.shape == (4, 3)
    assert is_arr(dt1)
    assert_valueerror(dt0, (i if i % 3 < 2 else str(-i) for i in range(10)),
                      "Invalid row selector '-2' generated at position 2")


def test_rows_multislice(dt0):
    """
    Test that it is possible to combine multiple row-selectors in an array:
        dt([0, 5, 2, 1])
        dt([slice(None, None, 2), slice(1, None, 2)])
        dt([0, 1] * 100)
    """
    for selector, nrows in [([2, 7, 0, 9], 4),
                            ({1, -1, 0}, 3),
                            ((-1, -1, -1, -1), 4),
                            ([slice(5, None), slice(None, 5)], 10),
                            ([0, 2, range(4), -1], 7),
                            ([4, 9, 3, slice(7), range(10)], 20)]:
        dt1 = dt0(selector)
        assert dt1.shape == (nrows, 3)
        assert dt1.names == ("colA", "colB", "colC")
        assert dt1.types == ("bool", "int", "real")
        assert is_arr(dt1)
    assert as_list(dt0([3, 9, 1, 0]))[0] == [None, 1, 1, 0]
    assert as_list(dt0((2, 5, 5, -1)))[1] == [9, 0, 0, None]
    assert (as_list(dt0([slice(5, None), slice(None, 5)]))[1] ==
            [0, 0, -1, 1, None, 7, -11, 9, 10000, None])
    assert (as_list(dt0([3, 1, slice(-3), 9, 9, 9]))[2] ==
            [0.1, 1, 5, 1, 1.3, 0.1, 100000, 0, -2.6, 2, 2, 2])
    assert_valueerror(
        dt0, [1, "hey"],
        "Invalid row selector 'hey' at element 1 of the `rows` list")
    assert_valueerror(dt0, [1, -1, 5, -11], "Row `-11` is invalid")



def test_rows_column(dt0):
    """
    Test the use of a boolean column as a filter:
        dt(rows=dt2[0])
    """
    col = dt.DataTable([1, 0, 1, 1, None, 0, None, 1, 1, 0])
    dt1 = dt0(col)
    assert dt1.shape == (5, 3)
    assert dt1.names == ("colA", "colB", "colC")
    assert dt1.types == ("bool", "int", "real")
    assert is_arr(dt1)
    assert as_list(dt1)[1] == [7, 9, 10000, -1, 1]
    assert_valueerror(dt0, dt0,
                      "`rows` argument should be a single-column datatable")
    assert_typeerror(dt0, dt.DataTable(list(range(10))),
                     "`rows` datatable should be a boolean column")
    assert_valueerror(dt0, dt.DataTable(list(i % 2 for i in range(20))),
                      "`rows` datatable has 20 rows, but applied to a "
                      "datatable with 10 rows")


def test_rows_function(dt0):
    """
    Test that it is possible to use a lambda-expression as a filter:
        dt(lambda f: f.colA < f.colB)
    """
    dt1 = dt0(lambda f: f.colA)
    assert dt1.shape == (5, 3)
    assert is_arr(dt1)
    assert as_list(dt1)[1] == [-11, 9, 0, 1, None]

    dt2 = dt0(lambda f: [5, 7, 9])
    assert dt2.shape == (3, 3)
    assert as_list(dt2)[1] == [0, -1, None]

    dt3 = dt0(lambda f: f.colA < f.colB)
    assert dt3.shape == (2, 3)
    assert as_list(dt3) == [[0, 1], [7, 9], [5, 1.3]]

    dt4 = dt0(lambda f: f.colB == 0)
    assert dt4.shape == (2, 3)
    assert as_list(dt4) == [[0, 1], [0, 0], [0, -2.6]]

    assert_typeerror(dt0, lambda f: "boooo!",
                     "Unexpected result produced by the `rows` function")



@pytest.mark.skip(reason="Need to implement columnsets over view datatables")
def test_chained_slice(dt0):
    dt1 = dt0[::2, :]
    assert dt1.shape == (5, 3)
    assert dt1.internal.rowmapping_type == "slice"
    dt2 = dt1[::-1, :]
    assert dt2.shape == (5, 3)
    assert dt2.internal.rowmapping_type == "slice"
    assert as_list(dt2)[1] == [1, 0, None, 9, 7]
    dt3 = dt1[2:4, :]
    assert dt3.shape == (2, 3)
    assert dt3.internal.rowmapping_type == "slice"
    assert as_list(dt3) == [[0, 1], [None, 0], [100000, -2.6]]
    dt4 = dt1[(1, 0, 3, 2), :]
    assert dt4.shape == (4, 3)
    assert dt4.internal.rowmapping_type == "arr32"
    assert as_list(dt4) == [[1, 0, 1, 0], [9, 7, 0, None], [1.3, 5, -2.6, 1e5]]


@pytest.mark.skip(reason="Need to implement columnsets over view datatables")
def test_chained_array(dt0):
    dt1 = dt0[(2, 5, 1, 1, 1, 0), :]
    assert dt1.shape == (6, 3)
    assert dt1.internal.rowmapping_type == "arr32"
    dt2 = dt1[::2, :]
    assert dt2.shape == (3, 3)
    assert dt2.internal.rowmapping_type == "arr32"
    assert as_list(dt2) == [[1, 1, 1], [9, -11, -11], [1.3, 1, 1]]
    dt3 = dt1[::-1, :]
    assert dt3.shape == (6, 3)
    assert dt3.internal.rowmapping_type == "arr32"
    assert as_list(dt3)[:2] == [[0, 1, 1, 1, 0, 1], [7, -11, -11, -11, 0, 9]]
    dt4 = dt1[(2, 3, 0), :]
    assert dt4.shape == (3, 3)
    assert as_list(dt4) == [[1, 1, 1], [-11, -11, 9], [1, 1, 1.3]]


def test_rows_bad_arguments(dt0):
    """
    Test row selectors that are invalid (i.e. not of the types listed above).
    """
    assert_typeerror(dt0, 0.5, "Unexpected `rows` argument: 0.5")
    assert_typeerror(dt0, "5", "Unexpected `rows` argument: '5'")
