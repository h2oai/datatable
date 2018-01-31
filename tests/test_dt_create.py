#!/usr/bin/env python
# © H2O.ai 2018; -*- encoding: utf-8 -*-
#   This Source Code Form is subject to the terms of the Mozilla Public
#   License, v. 2.0. If a copy of the MPL was not distributed with this
#   file, You can obtain one at http://mozilla.org/MPL/2.0/.
#-------------------------------------------------------------------------------
#
# Test creating a datatable from various sources
#
#-------------------------------------------------------------------------------
import pytest
import datatable as dt
from datatable import ltype, stype
from tests import same_iterables, list_equals, assert_equals


def test_create_from_list():
    d0 = dt.DataTable([1, 2, 3])
    assert d0.shape == (3, 1)
    assert d0.names == ("C1", )
    assert d0.ltypes == (ltype.int, )
    assert d0.internal.check()


def test_create_from_list_of_lists():
    d1 = dt.DataTable([[1, 2], [True, False], [.3, -0]], names="ABC")
    assert d1.shape == (2, 3)
    assert d1.names == ("A", "B", "C")
    assert d1.ltypes == (ltype.int, ltype.bool, ltype.real)
    assert d1.internal.check()


def test_create_from_tuple():
    d2 = dt.DataTable((3, 5, 6, 0))
    assert d2.shape == (4, 1)
    assert d2.ltypes == (ltype.int, )
    assert d2.internal.check()


def test_create_from_set():
    d3 = dt.DataTable({1, 13, 15, -16, -10, 7, 9, 1})
    assert d3.shape == (7, 1)
    assert d3.ltypes == (ltype.int, )
    assert d3.internal.check()


def test_create_from_range():
    d0 = dt.DataTable(range(8))
    assert d0.internal.check()
    assert d0.shape == (8, 1)
    assert d0.topython() == [list(range(8))]


def test_create_from_list_of_ranges():
    d0 = dt.DataTable([range(6), range(0, 12, 2)])
    assert d0.internal.check()
    assert d0.shape == (6, 2)
    assert d0.topython() == [list(range(6)), list(range(0, 12, 2))]


def test_create_from_nothing():
    d4 = dt.DataTable()
    assert d4.shape == (0, 0)
    assert d4.names == tuple()
    assert d4.ltypes == tuple()
    assert d4.stypes == tuple()
    assert d4.internal.check()


def test_create_from_empty_list():
    d5 = dt.DataTable([])
    assert d5.shape == (0, 1)
    assert d5.names == ("C1", )
    assert d5.ltypes == (ltype.bool, )
    assert d5.stypes == (stype.bool8, )
    assert d5.internal.check()


def test_create_from_empty_list_of_lists():
    d6 = dt.DataTable([[]])
    assert d6.shape == (0, 1)
    assert d6.names == ("C1", )
    assert d6.ltypes == (ltype.bool, )
    assert d6.internal.check()


def test_create_from_dict():
    d7 = dt.DataTable({"A": [1, 5, 10],
                       "B": [True, False, None],
                       "C": ["alpha", "beta", "gamma"]})
    assert d7.shape == (3, 3)
    assert same_iterables(d7.names, ("A", "B", "C"))
    assert same_iterables(d7.ltypes, (ltype.int, ltype.bool, ltype.str))
    assert d7.internal.check()


def test_create_from_datatable():
    d8_0 = dt.DataTable({"A": [1, 4, 3],
                         "B": [False, True, False],
                         "C": ["str1", "str2", "str3"]})
    d8_1 = dt.DataTable(d8_0)
    d8_2 = dt.DataTable(d8_0.internal, d8_0.names)
    assert_equals(d8_0, d8_1)
    assert_equals(d8_0, d8_2)


def test_create_from_string():
    d0 = dt.DataTable("""
        A,B,C,D
        1,2,3,boo
        0,5.5,,bar
        ,NaN,1000,""
    """)
    assert d0.internal.check()
    assert d0.names == ("A", "B", "C", "D")
    assert d0.ltypes == (dt.ltype.bool, dt.ltype.real, dt.ltype.int,
                         dt.ltype.str)
    assert d0.topython() == [[True, False, None], [2.0, 5.5, None],
                             [3, None, 1000], ["boo", "bar", ""]]


#-------------------------------------------------------------------------------
# Create from Pandas
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


def test_create_from_pandas_with_names(pandas):
    p = pandas.DataFrame({"A": [2, 5, 8], "B": ["e", "r", "qq"]})
    d = dt.DataTable(p, names=["miniature", "miniscule"])
    assert d.shape == (3, 2)
    assert same_iterables(d.names, ("miniature", "miniscule"))
    assert d.internal.check()


def test_create_from_pandas_series_with_names(pandas):
    p = pandas.Series([10000, 5, 19, -12])
    d = dt.DataTable(p, names=["ha!"])
    assert d.internal.check()
    assert d.shape == (4, 1)
    assert d.names == ("ha!", )
    assert d.topython() == [[10000, 5, 19, -12]]



#-------------------------------------------------------------------------------
# Create from Numpy
#-------------------------------------------------------------------------------

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
    assert d.shape == a.shape
    assert d.names == ("C1", "C2", "C3", "C4", "C5")
    assert d.internal.check()
    assert d.topython() == a.T.tolist()


def test_create_from_3d_numpy_array(numpy):
    a = numpy.array([[[1, 2, 3]]])
    with pytest.raises(ValueError) as e:
        dt.DataTable(a)
    assert "Cannot create DataTable from a 3-D numpy array" in str(e.value)


def test_create_from_string_numpy_array(numpy):
    a = numpy.array(["alef", "bet", "gimel", "dalet", "he", "юйґї"])
    d = dt.DataTable(a)
    assert d.internal.check()
    assert d.shape == (6, 1)
    assert d.names == ("C1", )
    assert d.topython() == [a.tolist()]


def test_create_from_masked_numpy_array1(numpy):
    a = numpy.array([True, False, True, False, True])
    m = numpy.array([False, True, False, False, True])
    arr = numpy.ma.array(a, mask=m)
    d = dt.DataTable(arr)
    assert d.shape == (5, 1)
    assert d.stypes == (stype.bool8, )
    assert d.internal.check()
    assert d.topython() == [[True, None, True, False, None]]


def test_create_from_masked_numpy_array2(numpy):
    n = 100
    a = numpy.random.randn(n) * 1000
    m = numpy.random.randn(n) > 1
    arr = numpy.ma.array(a, mask=m, dtype="int16")
    d = dt.DataTable(arr)
    assert d.shape == (n, 1)
    assert d.stypes == (stype.int16, )
    assert d.internal.check()
    assert d.topython() == [arr.tolist()]


def test_create_from_masked_numpy_array3(numpy):
    n = 100
    a = numpy.random.randn(n) * 1e6
    m = numpy.random.randn(n) > 1
    arr = numpy.ma.array(a, mask=m, dtype="int32")
    d = dt.DataTable(arr)
    assert d.shape == (n, 1)
    assert d.stypes == (stype.int32, )
    assert d.internal.check()
    assert d.topython() == [arr.tolist()]


def test_create_from_masked_numpy_array4(numpy):
    n = 10
    a = numpy.random.randn(n) * 10
    m = numpy.random.randn(n) > 1
    arr = numpy.ma.array(a, mask=m, dtype="float64")
    d = dt.DataTable(arr)
    assert d.shape == (n, 1)
    assert d.stypes == (stype.float64, )
    assert d.internal.check()
    assert list_equals(d.topython(), [arr.tolist()])


def test_create_from_numpy_array_with_names(numpy):
    a = numpy.array([1, 2, 3])
    d = dt.DataTable(a, names=["gargantuan"])
    assert d.shape == (3, 1)
    assert d.names == ("gargantuan", )
    assert d.internal.check()
    assert d.topython() == [[1, 2, 3]]



#-------------------------------------------------------------------------------
# Others
#-------------------------------------------------------------------------------

def test_bad():
    with pytest.raises(TypeError) as e:
        dt.DataTable(1)
    assert "Cannot create DataTable from 1" in str(e.value)
    with pytest.raises(TypeError) as e:
        dt.DataTable(dt)
    assert "Cannot create DataTable from <module 'datatable'" in str(e.value)


def test_issue_42():
    d = dt.DataTable([-1])
    assert d.shape == (1, 1)
    assert d.ltypes == (ltype.int, )
    assert d.internal.check()
    d = dt.DataTable([-1, 2, 5, "hooray"])
    assert d.shape == (4, 1)
    assert d.ltypes == (ltype.obj, )
    assert d.internal.check()


def test_issue_409():
    from math import inf, copysign
    d = dt.DataTable([10**333, -10**333, 10**-333, -10**-333])
    assert d.internal.check()
    assert d.ltypes == (ltype.real, )
    p = d.topython()
    assert p == [[inf, -inf, 0.0, -0.0]]
    assert copysign(1, p[0][-1]) == -1


def test_duplicate_names1():
    with pytest.warns(UserWarning) as ws:
        d = dt.DataTable([[1], [2], [3]], names=["A", "A", "A"])
        assert d.names == ("A", "A.1", "A.2")
    assert len(ws) == 1
    assert "Duplicate column names found: ['A', 'A']" in ws[0].message.args[0]


def test_duplicate_names2():
    with pytest.warns(UserWarning):
        d = dt.DataTable([[1], [2], [3], [4]], names=("A", "A.1", "A", "A.2"))
        assert d.names == ("A", "A.1", "A.2", "A.3")


def test_special_characters_in_names():
    d = dt.DataTable([[1], [2], [3], [4]],
                     names=("".join(chr(i) for i in range(32)),
                            "help\nneeded",
                            "foo\t\tbar\t \tbaz",
                            "A\n\rB\n\rC\n\rD\n\r"))
    assert d.names == (".", "help.needed", "foo.bar. .baz", "A.B.C.D.")
