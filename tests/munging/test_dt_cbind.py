#!/usr/bin/env python3
# Copyright 2017 H2O.ai; Apache License Version 2.0;  -*- encoding: utf-8 -*-
import pytest
import types
import datatable as dt


#-------------------------------------------------------------------------------
# Prepare fixtures & helper functions
#-------------------------------------------------------------------------------

def assert_equals(datatable1, datatable2):
    nrows, ncols = datatable1.shape
    assert datatable1.shape == datatable2.shape
    assert datatable1.names == datatable2.names
    assert datatable1.types == datatable2.types
    data1 = datatable1.internal.window(0, nrows, 0, ncols).data
    data2 = datatable2.internal.window(0, nrows, 0, ncols).data
    assert data1 == data2
    assert datatable1.internal.verify_integrity() is None
    assert datatable2.internal.verify_integrity() is None



#-------------------------------------------------------------------------------
# Run the tests
#-------------------------------------------------------------------------------

def test_cbind_exists():
    dt0 = dt.DataTable([1, 2, 3])
    assert isinstance(dt0.cbind, types.MethodType)
    assert len(dt0.cbind.__doc__) > 2000


def test_cbind_simple():
    d0 = dt.DataTable([1, 2, 3])
    d1 = dt.DataTable([4, 5, 6])
    d0.cbind(d1)
    dr = dt.DataTable([[1, 2, 3], [4, 5, 6]], colnames=["C1", "C1"])
    assert_equals(d0, dr)


def test_cbind_empty():
    d0 = dt.DataTable([1, 2, 3])
    d0.cbind()
    assert_equals(d0, dt.DataTable([1, 2, 3]))


def test_cbind_self():
    d0 = dt.DataTable({"fun": [1, 2, 3]})
    d0.cbind(d0).cbind(d0).cbind(d0)
    dr = dt.DataTable([[1, 2, 3]] * 8, colnames=["fun"] * 8)
    assert_equals(d0, dr)


def test_cbind_notinplace():
    d0 = dt.DataTable({"A": [1, 2, 3]})
    d1 = dt.DataTable({"B": [4, 5, 6]})
    dd = d0.cbind(d1, inplace=False)
    dr = dt.DataTable({"A": [1, 2, 3], "B": [4, 5, 6]})
    assert_equals(dd, dr)
    assert_equals(d0, dt.DataTable({"A": [1, 2, 3]}))
    assert_equals(d1, dt.DataTable({"B": [4, 5, 6]}))


def test_cbind_notforced():
    d0 = dt.DataTable([1, 2, 3])
    d1 = dt.DataTable([4, 5])
    with pytest.raises(ValueError) as e:
        d0.cbind(d1)
    assert ("Cannot merge datatable with 2 rows to a datatable with 3 rows"
            in str(e.value))
    with pytest.raises(ValueError) as e:
        d0.cbind(dt.DataTable())
    assert ("Cannot merge datatable with 0 rows to a datatable with 3 rows"
            in str(e.value))


def test_cbind_forced1():
    d0 = dt.DataTable([1, 2, 3])
    d1 = dt.DataTable([4, 5])
    d0.cbind(d1, force=True)
    dr = dt.DataTable([[1, 2, 3], [4, 5, None]], colnames=["C1", "C1"])
    assert_equals(d0, dr)


def test_cbind_forced2():
    d0 = dt.DataTable({"A": [1, 2, 3], "B": [7, None, -1]})
    d1 = dt.DataTable({"C": list("abcdefghij")})
    d0.cbind(d1, force=True)
    dr = dt.DataTable({"A": [1, 2, 3] + [None] * 7,
                       "B": [7, None, -1] + [None] * 7,
                       "C": list("abcdefghij")})
    assert_equals(d0, dr)


def test_cbind_forced3():
    d0 = dt.DataTable({"A": list(range(10))})
    d1 = dt.DataTable({"B": ["one", "two", "three"]})
    d0.cbind(d1, force=True)
    dr = dt.DataTable({"A": list(range(10)),
                       "B": ["one", "two", "three"] + [None] * 7})
    assert_equals(d0, dr)


def test_cbind_onerow1():
    d0 = dt.DataTable({"A": [1, 2, 3, 4, 5]})
    d1 = dt.DataTable({"B": [100.0]})
    d0.cbind(d1)
    dr = dt.DataTable({"A": [1, 2, 3, 4, 5], "B": [100.0] * 5})
    assert_equals(d0, dr)


def test_cbind_onerow2():
    d0 = dt.DataTable({"A": ["mu"]})
    d1 = dt.DataTable({"B": [7, 9, 10, 15]})
    d0.cbind(d1)
    dr = dt.DataTable({"A": ["mu"] * 4, "B": [7, 9, 10, 15]})
    assert_equals(d0, dr)


def test_bad_arguments():
    d0 = dt.DataTable([1, 2, 3])
    d1 = dt.DataTable([5])
    with pytest.raises(TypeError):
        d0.cbind(100)
    with pytest.raises(TypeError):
        d0.cbind(d1, inplace=3)
    with pytest.raises(TypeError):
        d0.cbind(d1, force=None)


def test_cbind_views1():
    d0 = dt.DataTable({"A": list(range(100))})
    d1 = d0[:5, :]
    assert d1.internal.isview
    d2 = dt.DataTable({"B": [3, 6, 9, 12, 15]})
    d1.cbind(d2)
    assert not d1.internal.isview
    dr = dt.DataTable({"A": list(range(5)), "B": list(range(3, 18, 3))})
    assert_equals(d1, dr)


def test_cbind_views2():
    d0 = dt.DataTable({"A": list(range(10))})
    d1 = d0[2:5, :]
    assert d1.internal.isview
    d2 = dt.DataTable({"B": list("abcdefghij")})
    d3 = d2[-3:, :]
    assert d3.internal.isview
    d1.cbind(d3)
    assert not d1.internal.isview
    dr = dt.DataTable({"A": [2, 3, 4], "B": ["h", "i", "j"]})
    assert_equals(d1, dr)


def test_cbind_multiple():
    d0 = dt.DataTable({"A": [1, 2, 3]})
    d1 = dt.DataTable({"B": ["doo"]})
    d2 = dt.DataTable({"C": [True, False]})
    d3 = dt.DataTable({"D": [10, 9, 8, 7]})
    d4 = dt.DataTable({"E": [1]})[:0, :]
    d0.cbind(d1, d2, d3, d4, force=True)
    dr = dt.DataTable({"A": [1, 2, 3, None],
                       "B": ["doo", "doo", "doo", "doo"],
                       "C": [True, False, None, None],
                       "D": [10, 9, 8, 7],
                       "E": [None, None, None, None]})
    assert_equals(d0, dr)


def test_cbind_1row_none():
    # Special case: append a single-row string column containing value NA
    d0 = dt.DataTable({"A": [1, 2, 3]})
    d1 = dt.DataTable({"B": [None, "doo"]})[0, :]
    assert d1.shape == (1, 1)
    assert d1.stypes == ("i4s", )
    d0.cbind(d1)
    dr = dt.DataTable({"A": [1, 2, 3, 4], "B": [None, None, None, "f"]})[:-1, :]
    assert_equals(d0, dr)
