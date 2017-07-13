#!/usr/bin/env python3
# Copyright 2017 H2O.ai; Apache License Version 2.0;  -*- encoding: utf-8 -*-
import pytest
import types
from tests import datatable as dt


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


#-------------------------------------------------------------------------------
# Run the tests
#-------------------------------------------------------------------------------

def test_append_exists():
    dt0 = dt.DataTable([1, 2, 3])
    assert isinstance(dt0.append, types.MethodType)
    assert len(dt0.append.__doc__) > 2000


def test_append_simple():
    dt0 = dt.DataTable([1, 2, 3])
    dt1 = dt.DataTable([5, 9])
    dt0.append(dt1)
    assert_equals(dt0, dt.DataTable([1, 2, 3, 5, 9]))


def test_append_fails():
    dt0 = dt.DataTable({"A": [1, 2, 3]})
    dt1 = dt.DataTable({"A": [5], "B": [7]})
    dt2 = dt.DataTable({"C": [10, -1]})

    with pytest.raises(ValueError) as e:
        dt0.append(dt1)
    assert "Cannot append datatable with 2 columns to a datatable " \
           "with 1 column" in str(e.value)
    assert "`force=True`" in str(e.value)

    with pytest.raises(ValueError) as e:
        dt0.append(dt1, bynames=False)
    assert "Cannot append datatable with 2 columns to a datatable " \
           "with 1 column" in str(e.value)
    assert "`force=True`" in str(e.value)

    with pytest.raises(ValueError) as e:
        dt1.append(dt0)
    assert "Cannot append datatable with 1 column to a datatable " \
           "with 2 columns" in str(e.value)
    assert "`force=True`" in str(e.value)

    with pytest.raises(ValueError) as e:
        dt0.append(dt2)
    assert "Column 'C' is not found in the source datatable" in str(e.value)
    assert "`force=True`" in str(e.value)


def test_append_bynumbers():
    dt0 = dt.DataTable({"A": [1, 2, 3], "V": [7, 7, 0]})
    dt1 = dt.DataTable({"C": [10, -1], "Z": [3, -1]})
    dtr = dt.DataTable({"A": [1, 2, 3, 10, -1], "V": [7, 7, 0, 3, -1]})
    dt0.append(dt1, bynames=False)
    assert_equals(dt0, dtr)

    dt0 = dt.DataTable({"X": [8]})
    dtr = dt.DataTable({"X": [8, 10, -1], "Z": [None, 3, -1]})
    dt0.append(dt1, bynames=False, force=True)
    assert_equals(dt0, dtr)


def test_append_bynames():
    dt0 = dt.DataTable({"A": [1, 2, 3], "V": [7, 7, 0]})
    dt1 = dt.DataTable({"V": [13], "A": [77]})
    dtr = dt.DataTable({"A": [1, 2, 3, 77], "V": [7, 7, 0, 13]})
    dt0.append(dt1)
    assert_equals(dt0, dtr)

    dt0 = dt.DataTable({"A": [5, 1]})
    dt1 = dt.DataTable({"C": [4], "D": [10]})
    dt2 = dt.DataTable({"A": [-1], "D": [4]})
    dtr = dt.DataTable({"A": [5, 1, None, -1],
                        "C": [None, None, 4, None],
                        "D": [None, None, 10, 4]})
    dt0.append(dt1, dt2, force=True)
    assert_equals(dt0, dtr)

    dt0 = dt.DataTable({"A": [7, 4], "B": [-1, 1]})
    dt1 = dt.DataTable({"A": [3]})
    dt2 = dt.DataTable({"B": [4]})
    dtr = dt.DataTable({"A": [7, 4, 3, None], "B": [-1, 1, None, 4]})
    dt0.append(dt1, force=True)
    dt0.append(dt2, force=True)
    assert_equals(dt0, dtr)

    dt0 = dt.DataTable({"A": [13]})
    dt1 = dt.DataTable({"B": [6], "A": [3], "E": [7]})
    dtr = dt.DataTable({"A": [13, 3], "B": [None, 6], "E": [None, 7]})
    dt0.append(dt1, force=True)
    assert_equals(dt0, dtr)


def test_not_inplace():
    dt0 = dt.DataTable({"A": [5, 1], "B": [4, 4]})
    dt1 = dt.DataTable({"A": [22], "B": [11]})
    dtr = dt0.append(dt1, inplace=False)
    assert_equals(dtr, dt.DataTable({"A": [5, 1, 22], "B": [4, 4, 11]}))
    assert_equals(dt0, dt.DataTable({"A": [5, 1], "B": [4, 4]}))


def test_repeating_names():
    dt0 = dt.DataTable([[5], [6], [7], [4]], colnames=["x", "y", "x", "x"])
    dt1 = dt.DataTable([[4], [3], [2]], colnames=["y", "x", "x"])
    dtr = dt.DataTable([[5, 3], [6, 4], [7, 2], [4, None]], colnames="xyxx")
    dt0.append(dt1, force=True)
    assert_equals(dt0, dtr)

    dt0 = dt.DataTable({"a": [23]})
    dt1 = dt.DataTable([[2], [4], [8]], colnames="aaa")
    dtr = dt.DataTable([[23, 2], [None, 4], [None, 8]], colnames="aaa")
    dt0.append(dt1, force=True)
    assert_equals(dt0, dtr)

    dt0 = dt.DataTable([[22], [44], [88]], colnames="aba")
    dt1 = dt.DataTable([[2], [4], [8]], colnames="aaa")
    dtr = dt.DataTable([[22, 2], [44, None], [88, 4], [None, 8]],
                       colnames=["a", "b", "a", "a"])
    dt0.append(dt1, force=True)
    assert_equals(dt0, dtr)


def test_append_strings():
    dt0 = dt.DataTable({"A": ["Nothing's", "wrong", "with", "this", "world"]})
    dt1 = dt.DataTable({"A": ["something", "wrong", "with", "humans"]})
    dt0.append(dt1)
    dtr = dt.DataTable({"A": ["Nothing's", "wrong", "with", "this", "world",
                              "something", "wrong", "with", "humans"]})
    assert_equals(dt0, dtr)

    dt0 = dt.DataTable(["a", "bc", None])
    dt1 = dt.DataTable([None, "def", "g", None])
    dt0.append(dt1)
    dtr = dt.DataTable(["a", "bc", None, None, "def", "g", None])
    assert_equals(dt0, dtr)

    dt0 = dt.DataTable({"A": [1, 5], "B": ["ham", "eggs"]})
    dt1 = dt.DataTable({"A": [25], "C": ["spam"]})
    dt0.append(dt1, force=True)
    dtr = dt.DataTable({"A": [1, 5, 25], "B": ["ham", "eggs", None],
                        "C": [None, None, "spam"]})
    assert_equals(dt0, dtr)

    dt0 = dt.DataTable({"A": ["alpha", None], "C": ["eta", "theta"]})
    dt1 = dt.DataTable({"A": [None, "beta"], "B": ["gamma", "delta"]})
    dt2 = dt.DataTable({"D": ["psi", "omega"]})
    dt0.append(dt1, dt2, force=True)
    dtr = dt.DataTable({"A": ["alpha", None, None, "beta", None, None],
                        "C": ["eta", "theta", None, None, None, None],
                        "B": [None, None, "gamma", "delta", None, None],
                        "D": [None, None, None, None, "psi", "omega"]})
    assert_equals(dt0, dtr)


def test_append_self():
    dt0 = dt.DataTable({"A": [1, 5, 7], "B": ["one", "two", None]})
    dt0.append(dt0, dt0, dt0)
    dtr = dt.DataTable({"A": [1, 5, 7] * 4, "B": ["one", "two", None] * 4})
    assert_equals(dt0, dtr)


# TODO: add tests for appending memory-mapped datatables (Issue #65)
# TODO: add tests for appending datatables with different column types
# TODO: add tests for appending categorical columns (requires merging levelsets)
# TODO: add tests for appending slices of data
