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
import math
import pytest
import datatable as dt
from datatable import ltype, stype
from tests import same_iterables, list_equals, assert_equals



#-------------------------------------------------------------------------------
# Create from primitives
#-------------------------------------------------------------------------------

def test_create_from_list():
    d0 = dt.Frame([1, 2, 3])
    assert d0.shape == (3, 1)
    assert d0.names == ("C1", )
    assert d0.ltypes == (ltype.int, )
    assert d0.internal.check()


def test_create_from_list_of_lists():
    d1 = dt.Frame([[1, 2], [True, False], [.3, -0]], names="ABC")
    assert d1.shape == (2, 3)
    assert d1.names == ("A", "B", "C")
    assert d1.ltypes == (ltype.int, ltype.bool, ltype.real)
    assert d1.internal.check()


def test_create_from_tuple():
    d2 = dt.Frame((3, 5, 6, 0))
    assert d2.shape == (4, 1)
    assert d2.ltypes == (ltype.int, )
    assert d2.internal.check()


def test_create_from_set():
    d3 = dt.Frame({1, 13, 15, -16, -10, 7, 9, 1})
    assert d3.shape == (7, 1)
    assert d3.ltypes == (ltype.int, )
    assert d3.internal.check()


def test_create_from_range():
    d0 = dt.Frame(range(8))
    assert d0.internal.check()
    assert d0.shape == (8, 1)
    assert d0.topython() == [list(range(8))]


def test_create_from_list_of_ranges():
    d0 = dt.Frame([range(6), range(0, 12, 2)])
    assert d0.internal.check()
    assert d0.shape == (6, 2)
    assert d0.topython() == [list(range(6)), list(range(0, 12, 2))]


def test_create_from_nothing():
    d4 = dt.Frame()
    assert d4.shape == (0, 0)
    assert d4.names == tuple()
    assert d4.ltypes == tuple()
    assert d4.stypes == tuple()
    assert d4.internal.check()


def test_create_from_empty_list():
    d5 = dt.Frame([])
    assert d5.shape == (0, 1)
    assert d5.names == ("C1", )
    assert d5.ltypes == (ltype.bool, )
    assert d5.stypes == (stype.bool8, )
    assert d5.internal.check()


def test_create_from_empty_list_of_lists():
    d6 = dt.Frame([[]])
    assert d6.shape == (0, 1)
    assert d6.names == ("C1", )
    assert d6.ltypes == (ltype.bool, )
    assert d6.internal.check()


def test_create_from_dict():
    d7 = dt.Frame({"A": [1, 5, 10],
                   "B": [True, False, None],
                   "C": ["alpha", "beta", "gamma"]})
    assert d7.shape == (3, 3)
    assert same_iterables(d7.names, ("A", "B", "C"))
    assert same_iterables(d7.ltypes, (ltype.int, ltype.bool, ltype.str))
    assert d7.internal.check()


def test_create_from_datatable():
    d8_0 = dt.Frame({"A": [1, 4, 3],
                     "B": [False, True, False],
                     "C": ["str1", "str2", "str3"]})
    d8_1 = dt.Frame(d8_0)
    d8_2 = dt.Frame(d8_0.internal, d8_0.names)
    assert_equals(d8_0, d8_1)
    assert_equals(d8_0, d8_2)


def test_create_from_string():
    d0 = dt.Frame("""
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
# Create specific stypes
#-------------------------------------------------------------------------------

def test_create_from_nones():
    d0 = dt.Frame([None, None, None])
    assert d0.internal.check()
    assert d0.stypes == (stype.bool8, )
    assert d0.shape == (3, 1)


def test_create_as_int8():
    d0 = dt.Frame([1, None, -1, 1000, 2.7, "123", "boo"], stype=stype.int8)
    assert d0.internal.check()
    assert d0.stypes == (stype.int8, )
    assert d0.shape == (7, 1)
    assert d0.topython() == [[1, None, -1, -24, 2, 123, None]]


def test_create_as_int16():
    d0 = dt.Frame([1e50, 1000, None, "27", "?", True], stype=stype.int16)
    assert d0.internal.check()
    assert d0.stypes == (stype.int16, )
    assert d0.shape == (6, 1)
    # int(1e50) = 2407412430484045 * 2**115, which is ≡0 (mod 2**16)
    assert d0.topython() == [[0, 1000, None, 27, None, 1]]


def test_create_as_int32():
    d0 = dt.Frame([1, 2, 5, 3.14, (1, 2)], stype=stype.int32)
    assert d0.internal.check()
    assert d0.stypes == (stype.int32, )
    assert d0.shape == (5, 1)
    assert d0.topython() == [[1, 2, 5, 3, None]]


def test_create_as_float32():
    d0 = dt.Frame([1, 5, 2.6, "7.7777", -1.2e+50, 1.3e-50],
                  stype=stype.float32)
    assert d0.internal.check()
    assert d0.stypes == (stype.float32, )
    assert d0.shape == (6, 1)
    # Apparently, float "inf" converts into double "inf" when cast. Good!
    assert list_equals(d0.topython(), [[1, 5, 2.6, 7.7777, -math.inf, 0]])


def test_create_as_float64():
    d0 = dt.Frame([[1, 2, 3, 4, 5, None],
                   [2.7, "3.1", False, "foo", 10**1000, -12**321]],
                  stype=float)
    assert d0.internal.check()
    assert d0.stypes == (stype.float64, stype.float64)
    assert d0.shape == (6, 2)
    assert d0.topython() == [[1.0, 2.0, 3.0, 4.0, 5.0, None],
                             [2.7, 3.1, 0, None, math.inf, -math.inf]]


def test_create_as_str32():
    d0 = dt.Frame([1, 2.7, "foo", None, (3, 4)], stype=stype.str32)
    assert d0.internal.check()
    assert d0.stypes == (stype.str32, )
    assert d0.shape == (5, 1)
    assert d0.topython() == [["1", "2.7", "foo", None, "(3, 4)"]]


def test_create_as_str64():
    d0 = dt.Frame(range(10), stype=stype.str64)
    assert d0.internal.check()
    assert d0.stypes == (stype.str64, )
    assert d0.shape == (10, 1)
    assert d0.topython() == [[str(n) for n in range(10)]]



#-------------------------------------------------------------------------------
# Create from Pandas
#-------------------------------------------------------------------------------

def test_create_from_pandas(pandas):
    p = pandas.DataFrame({"A": [2, 5, 8], "B": ["e", "r", "qq"]})
    d = dt.Frame(p)
    assert d.shape == (3, 2)
    assert same_iterables(d.names, ("A", "B"))
    assert d.internal.check()


def test_create_from_pandas2(pandas, numpy):
    p = pandas.DataFrame(numpy.ones((3, 5)))
    d = dt.Frame(p)
    assert d.shape == (3, 5)
    assert d.names == ("0", "1", "2", "3", "4")
    assert d.internal.check()


def test_create_from_pandas_series(pandas):
    p = pandas.Series([1, 5, 9, -12])
    d = dt.Frame(p)
    assert d.shape == (4, 1)
    assert d.internal.check()
    assert d.topython() == [[1, 5, 9, -12]]


def test_create_from_pandas_with_names(pandas):
    p = pandas.DataFrame({"A": [2, 5, 8], "B": ["e", "r", "qq"]})
    d = dt.Frame(p, names=["miniature", "miniscule"])
    assert d.shape == (3, 2)
    assert same_iterables(d.names, ("miniature", "miniscule"))
    assert d.internal.check()


def test_create_from_pandas_series_with_names(pandas):
    p = pandas.Series([10000, 5, 19, -12])
    d = dt.Frame(p, names=["ha!"])
    assert d.internal.check()
    assert d.shape == (4, 1)
    assert d.names == ("ha!", )
    assert d.topython() == [[10000, 5, 19, -12]]


def test_create_from_pandas_float16(pandas):
    src = [1.5, 2.6, 7.8]
    p = pandas.Series(src, dtype="float16")
    d = dt.Frame(p)
    assert d.internal.check()
    assert d.stypes == (stype.float32, )
    assert d.shape == (3, 1)
    # The precision of `float16`s is too low for `list_equals()` method.
    res = d.topython()[0]
    assert all(abs(src[i] - res[i]) < 1e-3 for i in range(3))



#-------------------------------------------------------------------------------
# Create from Numpy
#-------------------------------------------------------------------------------

def test_create_from_0d_numpy_array(numpy):
    a = numpy.array(100)
    d = dt.Frame(a)
    assert d.shape == (1, 1)
    assert d.names == ("C1", )
    assert d.internal.check()
    assert d.topython() == [[100]]


def test_create_from_1d_numpy_array(numpy):
    a = numpy.array([1, 2, 3])
    d = dt.Frame(a)
    assert d.shape == (3, 1)
    assert d.names == ("C1", )
    assert d.internal.check()
    assert d.topython() == [[1, 2, 3]]


def test_create_from_2d_numpy_array(numpy):
    a = numpy.array([[5, 4, 3, 10, 12], [-2, -1, 0, 1, 7]])
    d = dt.Frame(a)
    assert d.shape == a.shape
    assert d.names == ("C1", "C2", "C3", "C4", "C5")
    assert d.internal.check()
    assert d.topython() == a.T.tolist()


def test_create_from_3d_numpy_array(numpy):
    a = numpy.array([[[1, 2, 3]]])
    with pytest.raises(ValueError) as e:
        dt.Frame(a)
    assert "Cannot create Frame from a 3-D numpy array" in str(e.value)


def test_create_from_string_numpy_array(numpy):
    a = numpy.array(["alef", "bet", "gimel", "dalet", "he", "юйґї"])
    d = dt.Frame(a)
    assert d.internal.check()
    assert d.shape == (6, 1)
    assert d.names == ("C1", )
    assert d.topython() == [a.tolist()]


def test_create_from_masked_numpy_array1(numpy):
    a = numpy.array([True, False, True, False, True])
    m = numpy.array([False, True, False, False, True])
    arr = numpy.ma.array(a, mask=m)
    d = dt.Frame(arr)
    assert d.shape == (5, 1)
    assert d.stypes == (stype.bool8, )
    assert d.internal.check()
    assert d.topython() == [[True, None, True, False, None]]


def test_create_from_masked_numpy_array2(numpy):
    n = 100
    a = numpy.random.randn(n) * 1000
    m = numpy.random.randn(n) > 1
    arr = numpy.ma.array(a, mask=m, dtype="int16")
    d = dt.Frame(arr)
    assert d.shape == (n, 1)
    assert d.stypes == (stype.int16, )
    assert d.internal.check()
    assert d.topython() == [arr.tolist()]


def test_create_from_masked_numpy_array3(numpy):
    n = 100
    a = numpy.random.randn(n) * 1e6
    m = numpy.random.randn(n) > 1
    arr = numpy.ma.array(a, mask=m, dtype="int32")
    d = dt.Frame(arr)
    assert d.shape == (n, 1)
    assert d.stypes == (stype.int32, )
    assert d.internal.check()
    assert d.topython() == [arr.tolist()]


def test_create_from_masked_numpy_array4(numpy):
    n = 10
    a = numpy.random.randn(n) * 10
    m = numpy.random.randn(n) > 1
    arr = numpy.ma.array(a, mask=m, dtype="float64")
    d = dt.Frame(arr)
    assert d.shape == (n, 1)
    assert d.stypes == (stype.float64, )
    assert d.internal.check()
    assert list_equals(d.topython(), [arr.tolist()])


def test_create_from_numpy_array_with_names(numpy):
    a = numpy.array([1, 2, 3])
    d = dt.Frame(a, names=["gargantuan"])
    assert d.shape == (3, 1)
    assert d.names == ("gargantuan", )
    assert d.internal.check()
    assert d.topython() == [[1, 2, 3]]


def test_create_from_numpy_float16(numpy):
    src = [11.11, -3.162, 4.93, 0, 17.2]
    a = numpy.array(src, dtype="float16")
    d = dt.Frame(a)
    assert d.internal.check()
    assert d.stypes == (stype.float32, )
    assert d.shape == (len(src), 1)
    # The precision of `float16`s is too low for `list_equals()` method.
    res = d.topython()[0]
    assert all(abs(src[i] - res[i]) < 1e-3 for i in range(3))




#-------------------------------------------------------------------------------
# Issues
#-------------------------------------------------------------------------------

def test_bad():
    with pytest.raises(TypeError) as e:
        dt.Frame(1)
    assert "Cannot create Frame from 1" in str(e.value)
    with pytest.raises(TypeError) as e:
        dt.Frame(dt)
    assert "Cannot create Frame from <module 'datatable'" in str(e.value)


def test_issue_42():
    d = dt.Frame([-1])
    assert d.shape == (1, 1)
    assert d.ltypes == (ltype.int, )
    assert d.internal.check()
    d = dt.Frame([-1, 2, 5, "hooray"])
    assert d.shape == (4, 1)
    assert d.ltypes == (ltype.obj, )
    assert d.internal.check()


def test_issue_409():
    from math import inf, copysign
    d = dt.Frame([10**333, -10**333, 10**-333, -10**-333])
    assert d.internal.check()
    assert d.ltypes == (ltype.real, )
    p = d.topython()
    assert p == [[inf, -inf, 0.0, -0.0]]
    assert copysign(1, p[0][-1]) == -1


def test_duplicate_names1():
    with pytest.warns(UserWarning) as ws:
        d = dt.Frame([[1], [2], [3]], names=["A", "A", "A"])
        assert d.names == ("A", "A.1", "A.2")
    assert len(ws) == 1
    assert "Duplicate column names found: ['A', 'A']" in ws[0].message.args[0]


def test_duplicate_names2():
    with pytest.warns(UserWarning):
        d = dt.Frame([[1], [2], [3], [4]], names=("A", "A.1", "A", "A.2"))
        assert d.names == ("A", "A.1", "A.2", "A.3")


def test_special_characters_in_names():
    d = dt.Frame([[1], [2], [3], [4]],
                 names=("".join(chr(i) for i in range(32)),
                        "help\nneeded",
                        "foo\t\tbar\t \tbaz",
                        "A\n\rB\n\rC\n\rD\n\r"))
    assert d.names == (".", "help.needed", "foo.bar. .baz", "A.B.C.D.")



#-------------------------------------------------------------------------------
# Deprecated
#-------------------------------------------------------------------------------

def test_create_datatable():
    """DataTable is old symbol for Frame."""
    d = dt.DataTable([1, 2, 3])
    assert d.__class__.__name__ == "Frame"
    assert d.internal.check()
    assert d.topython() == [[1, 2, 3]]
