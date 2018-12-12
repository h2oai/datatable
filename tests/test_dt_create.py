#!/usr/bin/env python
# -*- coding: utf-8 -*-
#-------------------------------------------------------------------------------
# Copyright 2018 H2O.ai
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
# Test creating a datatable from various sources
#
#-------------------------------------------------------------------------------
import math
import os
import pytest
import datatable as dt
from datatable import ltype, stype, DatatableWarning
from tests import same_iterables, list_equals, assert_equals


#-------------------------------------------------------------------------------
# Test wrong parameters
#-------------------------------------------------------------------------------

def test_stypes_stype():
    with pytest.raises(TypeError) as e:
        dt.Frame(stypes=[], stype="float32")
    assert ("You can pass either parameter `stypes` or `stype` to Frame() "
            "constructor, but not both at the same time" == str(e.value))


def test_bad_stype():
    with pytest.raises(TypeError) as e:
        dt.Frame(stype=-1)
    assert ("Invalid value for `stype` parameter in Frame() constructor" ==
            str(e.value))


def test_bad_stypes():
    with pytest.raises(TypeError) as e:
        dt.Frame([], stypes=2.5)
    assert ("Argument `stypes` in Frame() constructor should be a list of "
            "stypes, instead received <class 'float'>" == str(e.value))


def test_unknown_arg():
    with pytest.raises(TypeError) as e:
        dt.Frame([1], dtype="int32")
    assert ("Frame() constructor got an unexpected keyword argument 'dtype'" ==
            str(e.value))


@pytest.mark.usefixtures("py36")
def test_unknown_args():
    # Under py35 the order of kw parameters will be random, which will affect
    # the error message
    with pytest.raises(TypeError) as e:
        dt.Frame([1], A=1, B=2)
    assert ("Frame() constructor got 2 unexpected keyword arguments: "
            "'A' and 'B'" == str(e.value))
    with pytest.raises(TypeError) as e:
        dt.Frame([1], A=1, B=2, C=3)
    assert ("Frame() constructor got 3 unexpected keyword arguments: "
            "'A', 'B' and 'C'" == str(e.value))
    with pytest.raises(TypeError) as e:
        dt.Frame([1], A=1, B=2, C=3, D=4)
    assert ("Frame() constructor got 4 unexpected keyword arguments: "
            "'A', 'B', ..., 'D'" == str(e.value))
    with pytest.raises(TypeError) as e:
        dt.Frame([1], A=1, B=2, C=3, D=4, E=5)
    assert ("Frame() constructor got 5 unexpected keyword arguments: "
            "'A', 'B', ..., 'E'" == str(e.value))


def test_stypes_dict():
    with pytest.raises(TypeError) as e:
        dt.Frame([1, 2, 3], stypes={"A": float})
    assert ("When parameter `stypes` is a dictionary, column `names` must "
            "be explicitly specified" == str(e.value))


def test_create_from_set():
    with pytest.raises(TypeError) as e:
        dt.Frame({1, 13, 15, -16, -10, 7, 9, 1})
    assert ("Cannot create Frame from <class 'set'>" == str(e.value))


def test_wrong_source():
    with pytest.raises(TypeError) as e:
        dt.Frame(A=[1], B=2)
    assert ("Cannot create a column from <class 'int'>" == str(e.value))


def test_different_column_lengths():
    with pytest.raises(ValueError) as e:
        dt.Frame([range(10), [3, 4, 6]])
    assert ("Column 1 has different number of rows (3) than the preceding "
            "columns (10)" == str(e.value))



#-------------------------------------------------------------------------------
# Create empty Frame
#-------------------------------------------------------------------------------

def test_create_from_nothing():
    d0 = dt.Frame()
    d0.internal.check()
    assert d0.shape == (0, 0)
    assert d0.names == tuple()
    assert d0.ltypes == tuple()
    assert d0.stypes == tuple()


def test_create_from_none():
    d0 = dt.Frame(None)
    d0.internal.check()
    assert d0.shape == (0, 0)
    assert d0.names == tuple()
    assert d0.ltypes == tuple()
    assert d0.stypes == tuple()


def test_create_from_empty_list():
    d0 = dt.Frame([])
    d0.internal.check()
    assert d0.shape == (0, 0)
    assert d0.names == tuple()
    assert d0.ltypes == tuple()
    assert d0.stypes == tuple()


def test_create_from_empty_list_with_params():
    d0 = dt.Frame([], names=[], stypes=[])
    d0.internal.check()
    assert d0.shape == (0, 0)
    d1 = dt.Frame([], stype=stype.int64)
    d1.internal.check()
    assert d1.shape == (0, 0)


def test_create_from_nothing_with_names():
    d0 = dt.Frame(names=["A", "B"])
    d0.internal.check()
    assert d0.shape == (0, 2)
    assert d0.names == ("A", "B")


def test_create_from_empty_list_bad():
    with pytest.raises(ValueError) as e:
        dt.Frame([], stypes=["int32", "str32"])
    assert ("The `stypes` argument contains 2 elements, which is more than the "
            "number of columns being created (0)" in str(e.value))



#-------------------------------------------------------------------------------
# Create from a primitive list
#-------------------------------------------------------------------------------

def test_create_from_list():
    d0 = dt.Frame([1, 2, 3])
    d0.internal.check()
    assert d0.shape == (3, 1)
    assert d0.names == ("C0", )
    assert d0.ltypes == (ltype.int, )


def test_create_from_tuple():
    d2 = dt.Frame((3, 5, 6, 0))
    d2.internal.check()
    assert d2.shape == (4, 1)
    assert d2.ltypes == (ltype.int, )


def test_create_from_range():
    d0 = dt.Frame(range(8))
    d0.internal.check()
    assert d0.shape == (8, 1)
    assert d0.to_list() == [list(range(8))]



#-------------------------------------------------------------------------------
# Create from a list of lists
#-------------------------------------------------------------------------------

def test_create_from_list_of_lists():
    d1 = dt.Frame([[1, 2], [True, False], [.3, -0]], names=list("ABC"))
    d1.internal.check()
    assert d1.shape == (2, 3)
    assert d1.names == ("A", "B", "C")
    assert d1.ltypes == (ltype.int, ltype.bool, ltype.real)


def test_create_from_list_of_lists_with_stypes_dict():
    d0 = dt.Frame([[4], [9], [3]], names=("a", "b", "c"), stypes={"c": float})
    d0.internal.check()
    assert d0.names == ("a", "b", "c")
    assert d0.ltypes == (ltype.int, ltype.int, ltype.real)
    assert d0.to_list() == [[4], [9], [3.0]]


def test_create_from_list_of_lists_with_stypes_dict_bad():
    with pytest.raises(TypeError) as e:
        dt.Frame([[4], [9], [3]], stypes={"c": float})
    assert ("When parameter `stypes` is a dictionary, column `names` must be "
            "explicitly specified" == str(e.value))


def test_create_from_list_of_ranges():
    d0 = dt.Frame([range(6), range(0, 12, 2)])
    d0.internal.check()
    assert d0.shape == (6, 2)
    assert d0.to_list() == [list(range(6)), list(range(0, 12, 2))]


def test_create_from_empty_list_of_lists():
    d6 = dt.Frame([[]])
    d6.internal.check()
    assert d6.shape == (0, 1)
    assert d6.names == ("C0", )
    assert d6.ltypes == (ltype.bool, )



#-------------------------------------------------------------------------------
# Create from a dict
#-------------------------------------------------------------------------------

def test_create_from_dict():
    d7 = dt.Frame({"A": [1, 5, 10],
                   "B": [True, False, None],
                   "C": ["alpha", "beta", "gamma"]})
    assert d7.shape == (3, 3)
    assert same_iterables(d7.names, ("A", "B", "C"))
    assert same_iterables(d7.ltypes, (ltype.int, ltype.bool, ltype.str))
    d7.internal.check()


def test_create_from_dict_error():
    with pytest.raises(TypeError) as e:
        dt.Frame({"A": range(3), "B": list('abc')}, names=("x", "y"))
    assert ("Parameter `names` cannot be used when constructing a Frame from "
            "a dictionary" == str(e.value))



#-------------------------------------------------------------------------------
# Create from keyword arguments
#-------------------------------------------------------------------------------

def test_create_from_kwargs0():
    d0 = dt.Frame(varname=[1])
    d0.internal.check()
    assert d0.names == ("varname",)
    assert d0.to_list() == [[1]]


def test_create_from_kwargs1():
    d0 = dt.Frame(A=[1, 2, 3], B=[True, None, False], C=["a", "b", "c"])
    d0.internal.check()
    assert same_iterables(d0.names, ("A", "B", "C"))
    assert same_iterables(d0.to_list(), [[1, 2, 3],
                                         [True, None, False],
                                         ["a", "b", "c"]])


def test_create_from_kwargs2():
    d0 = dt.Frame(x=range(4), y=[1, 3, 8, 0], stypes=[dt.int64, dt.float32])
    d0.internal.check()
    assert d0.shape == (4, 2)
    assert same_iterables(d0.names, ("x", "y"))
    assert same_iterables(d0.stypes, (dt.int64, dt.float32))
    assert same_iterables(d0.to_list(), [[0, 1, 2, 3], [1, 3, 8, 0]])


def test_create_from_kwargs_error():
    with pytest.raises(TypeError) as e:
        dt.Frame(A=range(3), B=list('abc'), names=("x", "y"))
    assert ("Parameter `names` cannot be used when constructing a Frame from "
            "varkwd arguments" == str(e.value))




#-------------------------------------------------------------------------------
# Create from a Frame (copy)
#-------------------------------------------------------------------------------

def test_create_from_frame():
    d0 = dt.Frame(A=[1, 4, 3],
                  B=[False, True, False],
                  C=["str1", "str2", "str3"])
    d0.internal.check()
    d1 = dt.Frame(d0)
    d1.internal.check()
    assert_equals(d0, d1)
    # Now check that d1 is a true copy of d0, rather than reference
    del d1[:, "C"]
    d1.nrows = 10
    assert d1.nrows == 10
    assert d1.ncols == 2
    assert d0.nrows == 3
    assert d0.ncols == 3


def test_create_from_frame2():
    d0 = dt.Frame([range(5), [7] * 5, list("owbke")])
    d1 = dt.Frame(d0, names=["A", "B", "C"])
    d1.internal.check()
    assert d1.to_list() == d0.to_list()
    assert d1.stypes == d0.stypes
    assert d1.names != d0.names
    assert d1.names == ("A", "B", "C")


def test_create_from_frame_error():
    d0 = dt.Frame([[3], [5.78], ["af"]], names=("A", "B", "C"))
    d0.internal.check()
    with pytest.raises(TypeError) as e1:
        dt.Frame(d0, stypes=[stype.int64, stype.float64, stype.str64])
    assert ("Parameter `stypes` is not allowed when making a copy of a "
            "Frame" == str(e1.value))
    with pytest.raises(TypeError) as e2:
        dt.Frame(d0, stypes=[stype.str32])
    assert str(e1.value) == str(e2.value)



#-------------------------------------------------------------------------------
# Create from a string (fread)
#-------------------------------------------------------------------------------

def test_create_from_string():
    d0 = dt.Frame("""
        A,B,C,D
        1,2,3,boo
        0,5.5,,bar
        ,NaN,1000,""
    """)
    d0.internal.check()
    assert d0.names == ("A", "B", "C", "D")
    assert d0.ltypes == (dt.ltype.bool, dt.ltype.real, dt.ltype.int,
                         dt.ltype.str)
    assert d0.to_list() == [[True, False, None], [2.0, 5.5, None],
                            [3, None, 1000], ["boo", "bar", ""]]


def test_cannot_create_from_multiple_files(tempfile):
    file1 = tempfile + ".1.csv"
    file2 = tempfile + ".2.csv"
    file3 = tempfile + ".3.csv"
    try:
        with open(file1, "w") as o1, open(file2, "w") as o2, \
                open(file3, "w") as o3:
            o1.write("A,B\nfoo,2\n")
            o2.write("3\n4\n5\n6\n")
            o3.write("qw\n1\n2\n5\n")
        ff = dt.fread(tempfile + ".*.csv")
        assert isinstance(ff, dict)
        assert len(ff) == 3
        with pytest.raises(ValueError) as e:
            dt.Frame(tempfile + ".*.csv")
        assert ("Frame cannot be initialized from multiple source files"
                in str(e.value))
    finally:
        os.remove(file1)
        os.remove(file2)



#-------------------------------------------------------------------------------
# Create from a list of tuples (as rows)
#-------------------------------------------------------------------------------

def test_create_from_list_of_tuples1():
    d0 = dt.Frame([(1, 2.0, "foo"),
                   (3, 1.5, "zee"),
                   (9, 0.1, "xyx"),
                   (0, -10, None)])
    d0.internal.check()
    assert d0.shape == (4, 3)
    assert d0.ltypes == (ltype.int, ltype.real, ltype.str)
    assert d0.to_list() == [[1, 3, 9, 0],
                            [2.0, 1.5, 0.1, -10.0],
                            ["foo", "zee", "xyx", None]]


def test_create_from_list_of_tuples2():
    d0 = dt.Frame([(1, 3, 5)], names=["a", "b", "c"], stypes=[int, float, str])
    d0.internal.check()
    assert d0.shape == (1, 3)
    assert d0.ltypes == (ltype.int, ltype.real, ltype.str)
    assert d0.names == ("a", "b", "c")
    assert d0.to_list() == [[1], [3.0], ["5"]]


def test_create_from_list_of_tuples_bad():
    with pytest.raises(TypeError) as e:
        dt.Frame([(1, 2, 3), (3, 4, 5), "4, 5, 6"])
    assert ("The source is not a list of tuples: element 2 is a "
            "<class 'str'>" == str(e.value))

    with pytest.raises(ValueError) as e:
        dt.Frame([(1, 2, 3), (4, 5), (5, 6, 7)])
    assert ("Misshaped rows in Frame() constructor: row 1 contains 2 elements, "
            "while the previous row had 3 elements" == str(e.value))

    with pytest.raises(ValueError) as e:
        dt.Frame([(1, 2, 3)], names=["a", "b"])
    assert ("The `names` argument contains 2 elements, which is less than "
            "the number of columns being created (3)" == str(e.value))

    with pytest.raises(ValueError) as e:
        dt.Frame([(1, 2, 3)], stypes=(stype.float32,) * 10)
    assert ("The `stypes` argument contains 10 elements, which is more than "
            "the number of columns being created (3)" == str(e.value))


def test_create_from_list_of_namedtuples():
    from collections import namedtuple
    Person = namedtuple("Person", ["name", "age", "sex"])
    d0 = dt.Frame([Person("Grogg", 21, "M"),
                   Person("Alexx", 14, "M"),
                   Person("Fiona", 24, "F")])
    d0.internal.check()
    assert d0.shape == (3, 3)
    assert d0.names == ("name", "age", "sex")
    assert d0.ltypes == (ltype.str, ltype.int, ltype.str)
    assert d0.to_list() == [["Grogg", "Alexx", "Fiona"],
                            [21, 14, 24], ["M", "M", "F"]]


def test_create_from_list_of_namedtuples_names_override():
    from collections import namedtuple
    abc = namedtuple("ABC", ["a", "b", "c"])
    d0 = dt.Frame([abc(5, 6, 7), abc(3, 2, 1)], names=["x", "y", "z"])
    d0.internal.check()
    assert d0.shape == (2, 3)
    assert d0.names == ("x", "y", "z")
    assert d0.ltypes == (ltype.int,) * 3
    assert d0.to_list() == [[5, 3], [6, 2], [7, 1]]



#-------------------------------------------------------------------------------
# Create from a list of dictionaries (as rows)
#-------------------------------------------------------------------------------

@pytest.mark.usefixtures("py36")
def test_create_from_list_of_dicts1():
    d0 = dt.Frame([{"a": 5, "b": 7, "c": "Hey"},
                   {"a": 99},
                   {"a": -4, "c": "Yay", "d": 2.17},
                   {"d": 1e10}, {}])
    d0.internal.check()
    assert d0.shape == (5, 4)
    assert d0.names == ("a", "b", "c", "d")
    assert d0.ltypes == (ltype.int, ltype.int, ltype.str, ltype.real)
    assert d0.to_list() == [[5, 99, -4, None, None],
                            [7, None, None, None, None],
                            ["Hey", None, "Yay", None, None],
                            [None, None, 2.17, 1e10, None]]


@pytest.mark.usefixtures("py36")
def test_create_from_list_of_dicts2():
    d0 = dt.Frame([{"foo": 11, "bar": 34}, {"argh": 17, "foo": 4}, {"_": 0}])
    d0.internal.check()
    assert d0.shape == (3, 4)
    assert d0.names == ("foo", "bar", "argh", "_")
    assert d0.to_list() == [[11, 4, None], [34, None, None],
                            [None, 17, None], [None, None, 0]]


def test_create_from_list_of_dicts_with_names1():
    d0 = dt.Frame([{"a": 12, "b": 77797, "c": "Rose"},
                   {"a": 37},
                   {"a": 80, "c": "Lily", "d": 3.14159},
                   {"d": 1.7e10}, {}],
                  names=["c", "a", "d", "e"])
    d0.internal.check()
    assert d0.shape == (5, 4)
    assert d0.names == ("c", "a", "d", "e")
    assert d0.ltypes == (ltype.str, ltype.int, ltype.real, ltype.bool)
    assert d0.to_list() == [["Rose", None, "Lily", None, None],
                            [12, 37, 80, None, None],
                            [None, None, 3.14159, 1.7e10, None],
                            [None, None, None, None, None]]


def test_create_from_list_of_dicts_with_names2():
    d0 = dt.Frame([{"a": 5}, {"b": 6}, {"c": 11}, {}], names=[])
    d0.internal.check()
    assert d0.shape == (0, 0)


def test_create_from_list_of_dicts_bad1():
    with pytest.raises(TypeError) as e:
        dt.Frame([{"a": 5}, {"b": 6}, None, {"c": 11}], names=[])
    assert ("The source is not a list of dicts: element 2 is a "
            "<class 'NoneType'>" == str(e.value))
    with pytest.raises(TypeError) as e:
        dt.Frame([{"a": 5}, {"b": 6}, None, {"c": 11}])
    assert ("The source is not a list of dicts: element 2 is a "
            "<class 'NoneType'>" == str(e.value))


def test_create_from_list_of_dicts_bad2():
    with pytest.raises(TypeError) as e:
        dt.Frame([{"a": 11}, {1: 4}])
    assert ("Invalid data in Frame() constructor: row 1 dictionary contains "
            "a key of type <class 'int'>, only string keys are allowed" ==
            str(e.value))


def test_create_from_list_of_dicts_bad3():
    with pytest.raises(TypeError) as e:
        dt.Frame([{"a": 11}, {"b": 4}], stypes=[int, int])
    assert ("If the Frame() source is a list of dicts, then either the `names` "
            "list has to be provided explicitly, or `stypes` parameter has "
            "to be a dictionary (or missing)" == str(e.value))




#-------------------------------------------------------------------------------
# Create specific stypes
#-------------------------------------------------------------------------------

def test_create_from_nones():
    d0 = dt.Frame([None, None, None])
    d0.internal.check()
    assert d0.stypes == (stype.bool8, )
    assert d0.shape == (3, 1)


def test_create_as_int8():
    d0 = dt.Frame([1, None, -1, 1000, 2.7, "123", "boo"], stype=stype.int8)
    d0.internal.check()
    assert d0.stypes == (stype.int8, )
    assert d0.shape == (7, 1)
    assert d0.to_list() == [[1, None, -1, -24, 2, 123, None]]


def test_create_as_int16():
    d0 = dt.Frame([1e50, 1000, None, "27", "?", True], stype=stype.int16)
    d0.internal.check()
    assert d0.stypes == (stype.int16, )
    assert d0.shape == (6, 1)
    # int(1e50) = 2407412430484045 * 2**115, which is ≡0 (mod 2**16)
    assert d0.to_list() == [[0, 1000, None, 27, None, 1]]


def test_create_as_int32():
    d0 = dt.Frame([1, 2, 5, 3.14, (1, 2)], stype=stype.int32)
    d0.internal.check()
    assert d0.stypes == (stype.int32, )
    assert d0.shape == (5, 1)
    assert d0.to_list() == [[1, 2, 5, 3, None]]


def test_create_as_float32():
    d0 = dt.Frame([1, 5, 2.6, "7.7777", -1.2e+50, 1.3e-50],
                  stype=stype.float32)
    d0.internal.check()
    assert d0.stypes == (stype.float32, )
    assert d0.shape == (6, 1)
    # Apparently, float "inf" converts into double "inf" when cast. Good!
    assert list_equals(d0.to_list(), [[1, 5, 2.6, 7.7777, -math.inf, 0]])


def test_create_as_float64():
    d0 = dt.Frame([[1, 2, 3, 4, 5, None],
                   [2.7, "3.1", False, "foo", 10**1000, -12**321]],
                  stype=float)
    d0.internal.check()
    assert d0.stypes == (stype.float64, stype.float64)
    assert d0.shape == (6, 2)
    assert d0.to_list() == [[1.0, 2.0, 3.0, 4.0, 5.0, None],
                            [2.7, 3.1, 0, None, math.inf, -math.inf]]


def test_create_as_str32():
    d0 = dt.Frame([1, 2.7, "foo", None, (3, 4)], stype=stype.str32)
    d0.internal.check()
    assert d0.stypes == (stype.str32, )
    assert d0.shape == (5, 1)
    assert d0.to_list() == [["1", "2.7", "foo", None, "(3, 4)"]]


def test_create_as_str64():
    d0 = dt.Frame(range(10), stype=stype.str64)
    d0.internal.check()
    assert d0.stypes == (stype.str64, )
    assert d0.shape == (10, 1)
    assert d0.to_list() == [[str(n) for n in range(10)]]


@pytest.mark.parametrize("st", [stype.int8, stype.int16, stype.int64,
                                stype.float32, stype.float64, stype.str32])
def test_create_range_as_stype(st):
    d0 = dt.Frame(range(10), stype=st)
    d0.internal.check()
    assert d0.stypes == (st,)
    if st == stype.str32:
        assert d0.to_list()[0] == [str(x) for x in range(10)]
    else:
        assert d0.to_list()[0] == list(range(10))



#-------------------------------------------------------------------------------
# Create with names
#-------------------------------------------------------------------------------

def test_create_names0():
    d1 = dt.Frame(range(10), names=["xyz"])
    d2 = dt.Frame(range(10), names=("xyz",))
    d1.internal.check()
    d2.internal.check()
    assert d1.shape == d2.shape == (10, 1)
    assert d1.names == d2.names == ("xyz",)


def test_create_names_bad1():
    with pytest.raises(ValueError) as e:
        dt.Frame(range(10), names=["a", "b"])
    assert ("The `names` argument contains 2 elements, which is more than the "
            "number of columns being created (1)" == str(e.value))


def test_create_names_bad2():
    with pytest.raises(TypeError) as e:
        dt.Frame([[1], [2], [3]], names="xyz")
    assert ("Argument `names` in Frame() constructor should be a list of "
            "strings, instead received <class 'str'>" == str(e.value))


def test_create_names_bad3():
    with pytest.raises(TypeError) as e:
        dt.Frame(range(5), names={"x": 1})
    assert ("Argument `names` in Frame() constructor should be a list of "
            "strings, instead received <class 'dict'>" == str(e.value))


def test_create_names_bad4():
    with pytest.raises(TypeError) as e:
        dt.Frame(range(5), names=[3])
    assert ("Invalid `names` list: element 0 is not a string"
            in str(e.value))



#-------------------------------------------------------------------------------
# Create from Pandas
#-------------------------------------------------------------------------------

def test_create_from_pandas(pandas):
    p = pandas.DataFrame({"A": [2, 5, 8], "B": ["e", "r", "qq"]})
    d = dt.Frame(p)
    d.internal.check()
    assert d.shape == (3, 2)
    assert same_iterables(d.names, ("A", "B"))


def test_create_from_pandas2(pandas, numpy):
    p = pandas.DataFrame(numpy.ones((3, 5)))
    d = dt.Frame(p)
    d.internal.check()
    assert d.shape == (3, 5)
    assert d.names == ("0", "1", "2", "3", "4")


def test_create_from_pandas_series(pandas):
    p = pandas.Series([1, 5, 9, -12])
    d = dt.Frame(p)
    d.internal.check()
    assert d.shape == (4, 1)
    assert d.to_list() == [[1, 5, 9, -12]]


def test_create_from_pandas_with_names(pandas):
    p = pandas.DataFrame({"A": [2, 5, 8], "B": ["e", "r", "qq"]})
    d = dt.Frame(p, names=["miniature", "miniscule"])
    d.internal.check()
    assert d.shape == (3, 2)
    assert same_iterables(d.names, ("miniature", "miniscule"))


def test_create_from_pandas_series_with_names(pandas):
    p = pandas.Series([10000, 5, 19, -12])
    d = dt.Frame(p, names=["ha!"])
    d.internal.check()
    assert d.shape == (4, 1)
    assert d.names == ("ha!", )
    assert d.to_list() == [[10000, 5, 19, -12]]


def test_create_from_pandas_float16_series(pandas):
    src = [1.5, 2.6, 7.8]
    p = pandas.Series(src, dtype="float16")
    d = dt.Frame(p)
    d.internal.check()
    assert d.stypes == (stype.float32, )
    assert d.shape == (3, 1)
    # The precision of `float16`s is too low for `list_equals()` method.
    res = d.to_list()[0]
    assert all(abs(src[i] - res[i]) < 1e-3 for i in range(3))


def test_create_from_pandas_float16_dataframe(pandas):
    p = pandas.DataFrame([[1, 3, 5], [7, 8, 9]], dtype="float16")
    d = dt.Frame(p)
    d.internal.check()
    assert d.stypes == (stype.float32,) * 3
    assert d.shape == (2, 3)


def test_create_from_pandas_issue1235(pandas):
    df = dt.fread("A\n" + "\U00010000" * 50).to_pandas()
    table = dt.Frame(df)
    table.internal.check()
    assert table.shape == (1, 1)
    assert table.scalar() == "\U00010000" * 50


def test_create_from_pandas_with_stypes(pandas):
    with pytest.raises(TypeError) as e:
        p = pandas.DataFrame([[1, 2, 3]])
        dt.Frame(p, stype=str)
    assert ("Argument `stypes` is not supported in Frame() constructor "
            "when creating a Frame from pandas DataFrame" == str(e.value))


def test_create_from_pandas_with_bad_names(pandas):
    with pytest.raises(ValueError) as e:
        p = pandas.DataFrame([[1, 2, 3]])
        dt.Frame(p, names=["A", "Z"])
    assert ("The `names` argument contains 2 elements, which is less than the "
            "number of columns being created (3)" == str(e.value))



#-------------------------------------------------------------------------------
# Create from Numpy
#-------------------------------------------------------------------------------

def test_create_from_0d_numpy_array(numpy):
    a = numpy.array(100)
    d = dt.Frame(a)
    d.internal.check()
    assert d.shape == (1, 1)
    assert d.names == ("C0", )
    assert d.to_list() == [[100]]


def test_create_from_1d_numpy_array(numpy):
    a = numpy.array([1, 2, 3])
    d = dt.Frame(a)
    d.internal.check()
    assert d.shape == (3, 1)
    assert d.names == ("C0", )
    assert d.to_list() == [[1, 2, 3]]


def test_create_from_2d_numpy_array(numpy):
    a = numpy.array([[5, 4, 3, 10, 12], [-2, -1, 0, 1, 7]])
    d = dt.Frame(a)
    d.internal.check()
    assert d.shape == a.shape
    assert d.names == ("C0", "C1", "C2", "C3", "C4")
    assert d.to_list() == a.T.tolist()


def test_create_from_3d_numpy_array(numpy):
    a = numpy.array([[[1, 2, 3]]])
    with pytest.raises(ValueError) as e:
        dt.Frame(a)
    assert "Cannot create Frame from a 3-D numpy array" in str(e.value)


def test_create_from_string_numpy_array(numpy):
    a = numpy.array(["alef", "bet", "gimel", "dalet", "he", "юйґї"])
    d = dt.Frame(a)
    d.internal.check()
    assert d.shape == (6, 1)
    assert d.names == ("C0", )
    assert d.to_list() == [a.tolist()]


def test_create_from_masked_numpy_array1(numpy):
    a = numpy.array([True, False, True, False, True])
    m = numpy.array([False, True, False, False, True])
    arr = numpy.ma.array(a, mask=m)
    d = dt.Frame(arr)
    d.internal.check()
    assert d.shape == (5, 1)
    assert d.stypes == (stype.bool8, )
    assert d.to_list() == [[True, None, True, False, None]]


def test_create_from_masked_numpy_array2(numpy):
    n = 100
    a = numpy.random.randn(n) * 1000
    m = numpy.random.randn(n) > 1
    arr = numpy.ma.array(a, mask=m, dtype="int16")
    d = dt.Frame(arr)
    d.internal.check()
    assert d.shape == (n, 1)
    assert d.stypes == (stype.int16, )
    assert d.to_list() == [arr.tolist()]


def test_create_from_masked_numpy_array3(numpy):
    n = 100
    a = numpy.random.randn(n) * 1e6
    m = numpy.random.randn(n) > 1
    arr = numpy.ma.array(a, mask=m, dtype="int32")
    d = dt.Frame(arr)
    d.internal.check()
    assert d.shape == (n, 1)
    assert d.stypes == (stype.int32, )
    assert d.to_list() == [arr.tolist()]


def test_create_from_masked_numpy_array4(numpy):
    n = 10
    a = numpy.random.randn(n) * 10
    m = numpy.random.randn(n) > 1
    arr = numpy.ma.array(a, mask=m, dtype="float64")
    d = dt.Frame(arr)
    d.internal.check()
    assert d.shape == (n, 1)
    assert d.stypes == (stype.float64, )
    assert list_equals(d.to_list(), [arr.tolist()])


def test_create_from_numpy_array_with_names(numpy):
    a = numpy.array([1, 2, 3])
    d = dt.Frame(a, names=["gargantuan"])
    d.internal.check()
    assert d.shape == (3, 1)
    assert d.names == ("gargantuan", )
    assert d.to_list() == [[1, 2, 3]]


def test_create_from_numpy_float16(numpy):
    src = [11.11, -3.162, 4.93, 0, 17.2]
    a = numpy.array(src, dtype="float16")
    d = dt.Frame(a)
    d.internal.check()
    assert d.stypes == (stype.float32, )
    assert d.shape == (len(src), 1)
    # The precision of `float16`s is too low for `list_equals()` method.
    res = d.to_list()[0]
    assert all(abs(src[i] - res[i]) < 1e-3 for i in range(3))


def test_create_from_list_of_numpy_arrays(numpy):
    df = dt.Frame([numpy.random.randint(2**30, size=100),
                   numpy.random.randn(100)], names=["A", "B"])
    df.internal.check()
    assert df.shape == (100, 2)
    assert df.names == ("A", "B")
    assert df.stypes == (stype.int64, stype.float64)


def test_create_from_dict_of_numpy_arrays(numpy):
    df = dt.Frame({"A": numpy.random.randn(67),
                   "B": numpy.random.randn(67),
                   "C": numpy.random.randn(67)})
    df.internal.check()
    assert df.shape == (67, 3)
    assert df.stypes == (stype.float64,) * 3
    assert same_iterables(df.names, ("A", "B", "C"))


def test_create_from_mixed_sources(numpy):
    df = dt.Frame({"A": numpy.random.randn(5),
                   "B": range(5),
                   "C": ["foo", "baw", "garrgh", "yex", "fin"],
                   "D": numpy.array([5, 8, 1, 3, 5813], dtype="int32")})
    df.internal.check()
    assert df.shape == (5, 4)
    assert same_iterables(df.names, ("A", "B", "C", "D"))
    assert same_iterables(df.stypes, (stype.float64, stype.int32, stype.str32,
                                      stype.int32))


def test_create_from_datetime_array(numpy):
    a = numpy.zeros(dtype="<M8[s]", shape=(10,))
    df = dt.Frame(a)
    df.internal.check()
    assert df.shape == (10, 1)
    assert df.stypes == (stype.str32,)
    assert df.to_list() == [["1970-01-01T00:00:00"] * 10]



#-------------------------------------------------------------------------------
# Issues
#-------------------------------------------------------------------------------

def test_bad():
    with pytest.raises(TypeError) as e:
        dt.Frame(1)
    assert "Cannot create Frame from <class 'int'>" == str(e.value)
    with pytest.raises(TypeError) as e:
        dt.Frame(dt)
    assert "Cannot create Frame from <class 'module'>" == str(e.value)


def test_ellipsis():
    d0 = dt.Frame(...)
    assert d0.names == ("?",)
    assert d0.shape == (1, 1)
    assert d0.scalar() == 42
    with pytest.raises(TypeError):
        dt.Frame(..., stype=float)
    with pytest.raises(TypeError):
        dt.Frame(..., stypes=[stype.str32])
    with pytest.raises(TypeError):
        dt.Frame(..., names=["?"])


def test_issue_42():
    d = dt.Frame([-1])
    d.internal.check()
    assert d.shape == (1, 1)
    assert d.ltypes == (ltype.int, )
    d = dt.Frame([-1, 2, 5, "hooray"])
    d.internal.check()
    assert d.shape == (4, 1)
    assert d.ltypes == (ltype.obj, )


def test_issue_409():
    from math import inf, copysign
    d = dt.Frame([10**333, -10**333, 10**-333, -10**-333])
    d.internal.check()
    assert d.ltypes == (ltype.real, )
    p = d.to_list()
    assert p == [[inf, -inf, 0.0, -0.0]]
    assert copysign(1, p[0][-1]) == -1


def test_duplicate_names1():
    with pytest.warns(DatatableWarning) as ws:
        d = dt.Frame([[1], [2], [3]], names=["A", "A", "A"])
        d.internal.check()
        assert d.names == ("A", "A.1", "A.2")
    assert len(ws) == 1
    assert "Duplicate column names found: 'A' and 'A'" in ws[0].message.args[0]


def test_duplicate_names2():
    with pytest.warns(DatatableWarning):
        d = dt.Frame([[1], [2], [3], [4]], names=("A", "A.1", "A", "A.2"))
        d.internal.check()
        assert d.names == ("A", "A.1", "A.2", "A.3")


def test_special_characters_in_names():
    d = dt.Frame([[1], [2], [3], [4]],
                 names=("".join(chr(i) for i in range(32)),
                        "help\nneeded",
                        "foo\t\tbar\t \tbaz",
                        "A\n\rB\n\rC\n\rD\n\r"))
    d.internal.check()
    assert d.names == (".", "help.needed", "foo.bar. .baz", "A.B.C.D.")



#-------------------------------------------------------------------------------
# Deprecated
#-------------------------------------------------------------------------------

def test_create_datatable():
    """DataTable is old symbol for Frame."""
    d = dt.DataTable([1, 2, 3])
    d.internal.check()
    assert d.__class__.__name__ == "Frame"
    assert d.to_list() == [[1, 2, 3]]
