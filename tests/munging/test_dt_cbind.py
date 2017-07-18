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


@pytest.mark.skip(reason="segfaults")
def test_cbind_forced3():
    d0 = dt.DataTable({"A": list(range(10))})
    d1 = dt.DataTable({"B": ["one", "two", "three"]})
    d0.cbind(d1, force=True)
    dr = dt.DataTable({"A": list(range(10)),
                       "B": ["one", "two", "three"] + [None] * 7})
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
