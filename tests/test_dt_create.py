#!/usr/bin/env python3
# Copyright 2017 H2O.ai; Apache License Version 2.0;  -*- encoding: utf-8 -*-
#-------------------------------------------------------------------------------
#
# Test creating a datatable from various sources
#
#-------------------------------------------------------------------------------
import pytest
import datatable as dt
from tests import same_iterables, list_equals


def test_create_from_list():
    d0 = dt.DataTable([1, 2, 3])
    assert d0.shape == (3, 1)
    assert d0.names == ("C1", )
    assert d0.types == ("int", )
    assert d0.internal.check()


def test_create_from_list_of_lists():
    d1 = dt.DataTable([[1, 2], [True, False], [.3, -0]], colnames="ABC")
    assert d1.shape == (2, 3)
    assert d1.names == ("A", "B", "C")
    assert d1.types == ("int", "bool", "real")
    assert d1.internal.check()


def test_create_from_tuple():
    d2 = dt.DataTable((3, 5, 6, 0))
    assert d2.shape == (4, 1)
    assert d2.types == ("int", )
    assert d2.internal.check()


def test_create_from_set():
    d3 = dt.DataTable({1, 13, 15, -16, -10, 7, 9, 1})
    assert d3.shape == (7, 1)
    assert d3.types == ("int", )
    assert d3.internal.check()


def test_create_from_nothing():
    d4 = dt.DataTable()
    assert d4.shape == (0, 0)
    assert d4.names == tuple()
    assert d4.types == tuple()
    assert d4.stypes == tuple()
    assert d4.internal.check()


def test_create_from_empty_list():
    d5 = dt.DataTable([])
    assert d5.shape == (0, 1)
    assert d5.names == ("C1", )
    assert d5.types == ("bool", )
    assert d5.stypes == ("i1b", )
    assert d5.internal.check()


def test_create_from_empty_list_of_lists():
    d6 = dt.DataTable([[]])
    assert d6.shape == (0, 1)
    assert d6.names == ("C1", )
    assert d6.types == ("bool", )
    assert d6.internal.check()


def test_create_from_dict():
    d7 = dt.DataTable({"A": [1, 5, 10],
                       "B": [True, False, None],
                       "C": ["alpha", "beta", "gamma"]})
    assert d7.shape == (3, 3)
    assert same_iterables(d7.names, ("A", "B", "C"))
    assert same_iterables(d7.types, ("int", "bool", "str"))
    assert d7.internal.check()



#-------------------------------------------------------------------------------
# Create from Pandas / Numpy
#-------------------------------------------------------------------------------

def test_create_from_pandas(pandas):
    p = pandas.DataFrame({"A": [2, 5, 8], "B": ["e", "r", "qq"]})
    d = dt.DataTable(p)
    assert d.shape == (3, 2)
    assert same_iterables(d.names, ("A", "B"))
    assert d.internal.check()


def test_create_from_pandas2(pandas, numpy):
    p = pandas.DataFrame(numpy.ones((3, 5)))
    d = dt.DataTable(p)
    assert d.shape == (3, 5)
    assert d.names == ("0", "1", "2", "3", "4")
    assert d.internal.check()


def test_create_from_pandas_series(pandas):
    p = pandas.Series([1, 5, 9, -12])
    d = dt.DataTable(p)
    assert d.shape == (4, 1)
    assert d.internal.check()
    assert d.topython() == [[1, 5, 9, -12]]


def test_create_from_0d_numpy_array(numpy):
    a = numpy.array(100)
    d = dt.DataTable(a)
    assert d.shape == (1, 1)
    assert d.names == ("C1", )
    assert d.internal.check()
    assert d.topython() == [[100]]


def test_create_from_1d_numpy_array(numpy):
    a = numpy.array([1, 2, 3])
    d = dt.DataTable(a)
    assert d.shape == (3, 1)
    assert d.names == ("C1", )
    assert d.internal.check()
    assert d.topython() == [[1, 2, 3]]


def test_create_from_2d_numpy_array(numpy):
    a = numpy.array([[5, 4, 3, 10, 12], [-2, -1, 0, 1, 7]])
    d = dt.DataTable(a)
    assert d.shape == (5, 2)
    assert d.names == ("C1", "C2")
    assert d.internal.check()
    assert d.topython() == a.tolist()


def test_create_from_3d_numpy_array(numpy):
    a = numpy.array([[[1, 2, 3]]])
    with pytest.raises(ValueError) as e:
        dt.DataTable(a)
    assert "Cannot create DataTable from a 3-D numpy array" in str(e.value)


@pytest.mark.skip(reason="Not implemented")
def test_create_from_string_numpy_array(numpy):
    a = numpy.array(["alef", "bet", "gimel", "dalet", "he", "vav"])
    d = dt.DataTable(a)
    assert d.shape == (6, 1)
    assert d.names == ("C1", )
    assert d.internal.check()
    assert d.topython() == [a.tolist()]


def test_create_from_masked_numpy_array1(numpy):
    a = numpy.array([True, False, True, False, True])
    m = numpy.array([False, True, False, False, True])
    arr = numpy.ma.array(a, mask=m)
    d = dt.DataTable(arr)
    assert d.shape == (5, 1)
    assert d.stypes == ("i1b", )
    assert d.internal.check()
    assert d.topython() == [[True, None, True, False, None]]


def test_create_from_masked_numpy_array2(numpy):
    n = 100
    a = numpy.random.randn(n) * 1000
    m = numpy.random.randn(n) > 1
    arr = numpy.ma.array(a, mask=m, dtype="int16")
    d = dt.DataTable(arr)
    assert d.shape == (n, 1)
    assert d.stypes == ("i2i", )
    assert d.internal.check()
    assert d.topython() == [arr.tolist()]


def test_create_from_masked_numpy_array3(numpy):
    n = 100
    a = numpy.random.randn(n) * 1e6
    m = numpy.random.randn(n) > 1
    arr = numpy.ma.array(a, mask=m, dtype="int32")
    d = dt.DataTable(arr)
    assert d.shape == (n, 1)
    assert d.stypes == ("i4i", )
    assert d.internal.check()
    assert d.topython() == [arr.tolist()]


def test_create_from_masked_numpy_array4(numpy):
    n = 10
    a = numpy.random.randn(n) * 10
    m = numpy.random.randn(n) > 1
    arr = numpy.ma.array(a, mask=m, dtype="float64")
    d = dt.DataTable(arr)
    assert d.shape == (n, 1)
    assert d.stypes == ("f8r", )
    assert d.internal.check()
    assert list_equals(d.topython(), [arr.tolist()])



#-------------------------------------------------------------------------------
# Others
#-------------------------------------------------------------------------------

def test_bad():
    with pytest.raises(TypeError) as e:
        dt.DataTable("scratch")
    assert "Cannot create DataTable from 'scratch'" in str(e.value)
    with pytest.raises(TypeError) as e:
        dt.DataTable(dt)
    assert "Cannot create DataTable from <module 'datatable'" in str(e.value)


def test_issue_42():
    d = dt.DataTable([-1])
    assert d.shape == (1, 1)
    assert d.types == ("int", )
    assert d.internal.check()
    d = dt.DataTable([-1, 2, 5, "hooray"])
    assert d.shape == (4, 1)
    assert d.types == ("str", )
    assert d.internal.check()
